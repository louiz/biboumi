#include <xmpp/biboumi_component.hpp>

#include <utils/timed_events.hpp>
#include <utils/scopeguard.hpp>
#include <utils/tolower.hpp>
#include <logger/logger.hpp>
#include <xmpp/adhoc_command.hpp>
#include <bridge/list_element.hpp>
#include <config/config.hpp>
#include <xmpp/jid.hpp>
#include <utils/sha1.hpp>

#include <stdexcept>
#include <iostream>

#include <stdio.h>

#include <louloulibs.h>

#include <uuid.h>

#ifdef SYSTEMD_FOUND
# include <systemd/sd-daemon.h>
#endif

using namespace std::string_literals;

static std::set<std::string> kickable_errors{
    "gone",
    "internal-server-error",
    "item-not-found",
    "jid-malformed",
    "recipient-unavailable",
    "redirect",
    "remote-server-not-found",
    "remote-server-timeout",
    "service-unavailable",
    "malformed-error"
    };


BiboumiComponent::BiboumiComponent(std::shared_ptr<Poller> poller, const std::string& hostname, const std::string& secret):
  XmppComponent(poller, hostname, secret)
{
  this->stanza_handlers.emplace("presence",
                                std::bind(&BiboumiComponent::handle_presence, this,std::placeholders::_1));
  this->stanza_handlers.emplace("message",
                                std::bind(&BiboumiComponent::handle_message, this,std::placeholders::_1));
  this->stanza_handlers.emplace("iq",
                                std::bind(&BiboumiComponent::handle_iq, this,std::placeholders::_1));

  this->adhoc_commands_handler.get_commands()= {
    {"ping", AdhocCommand({&PingStep1}, "Do a ping", false)},
    {"hello", AdhocCommand({&HelloStep1, &HelloStep2}, "Receive a custom greeting", false)},
    {"disconnect-user", AdhocCommand({&DisconnectUserStep1, &DisconnectUserStep2}, "Disconnect a user from the gateway", true)},
    {"reload", AdhocCommand({&Reload}, "Reload biboumi’s configuration", true)}
  };
}

void BiboumiComponent::shutdown()
{
  for (auto it = this->bridges.begin(); it != this->bridges.end(); ++it)
  {
    it->second->shutdown("Gateway shutdown");
  }
}

void BiboumiComponent::clean()
{
  auto it = this->bridges.begin();
  while (it != this->bridges.end())
  {
    it->second->clean();
    if (it->second->active_clients() == 0)
      it = this->bridges.erase(it);
    else
      ++it;
  }
}

void BiboumiComponent::handle_presence(const Stanza& stanza)
{
  std::string from = stanza.get_tag("from");
  std::string id = stanza.get_tag("id");
  std::string to_str = stanza.get_tag("to");
  std::string type = stanza.get_tag("type");

  // Check for mandatory tags
  if (from.empty())
    {
      log_warning("Received an invalid presence stanza: tag 'from' is missing.");
      return;
    }
  if (to_str.empty())
    {
      this->send_stanza_error("presence", from, this->served_hostname, id,
                              "modify", "bad-request", "Missing 'to' tag");
      return;
    }

  Bridge* bridge = this->get_user_bridge(from);
  Jid to(to_str);
  Iid iid(to.local);

  // An error stanza is sent whenever we exit this function without
  // disabling this scopeguard.  If error_type and error_name are not
  // changed, the error signaled is internal-server-error. Change their
  // value to signal an other kind of error. For example
  // feature-not-implemented, etc.  Any non-error process should reach the
  // stanza_error.disable() call at the end of the function.
  std::string error_type("cancel");
  std::string error_name("internal-server-error");
  utils::ScopeGuard stanza_error([&](){
      this->send_stanza_error("presence", from, to_str, id,
                              error_type, error_name, "");
        });

  if (iid.is_channel && !iid.get_server().empty())
    { // presence toward a MUC that corresponds to an irc channel, or a
      // dummy channel if iid.chan is empty
      if (type.empty())
        {
          const std::string own_nick = bridge->get_own_nick(iid);
          if (!own_nick.empty() && own_nick != to.resource)
            bridge->send_irc_nick_change(iid, to.resource);
          XmlNode* x = stanza.get_child("x", MUC_NS);
          XmlNode* password = x ? x->get_child("password", MUC_NS): nullptr;
          bridge->join_irc_channel(iid, to.resource,
                                   password ? password->get_inner() : "");
        }
      else if (type == "unavailable")
        {
          XmlNode* status = stanza.get_child("status", COMPONENT_NS);
          bridge->leave_irc_channel(std::move(iid), status ? std::move(status->get_inner()) : "");
        }
    }
  else
    {
      // An user wants to join an invalid IRC channel, return a presence error to him
      if (type.empty())
        this->send_invalid_room_error(to.local, to.resource, from);
    }
  stanza_error.disable();
}

void BiboumiComponent::handle_message(const Stanza& stanza)
{
  std::string from = stanza.get_tag("from");
  std::string id = stanza.get_tag("id");
  std::string to_str = stanza.get_tag("to");
  std::string type = stanza.get_tag("type");

  if (from.empty())
    return;
  if (type.empty())
    type = "normal";
  Bridge* bridge = this->get_user_bridge(from);
  Jid to(to_str);
  Iid iid(to.local);

  std::string error_type("cancel");
  std::string error_name("internal-server-error");
  utils::ScopeGuard stanza_error([&](){
      this->send_stanza_error("message", from, to_str, id,
                              error_type, error_name, "");
    });
  XmlNode* body = stanza.get_child("body", COMPONENT_NS);
  if (type == "groupchat" && iid.is_channel)
    {
      if (body && !body->get_inner().empty())
        {
          bridge->send_channel_message(iid, body->get_inner());
        }
      XmlNode* subject = stanza.get_child("subject", COMPONENT_NS);
      if (subject)
        bridge->set_channel_topic(iid, subject->get_inner());
    }
  else if (type == "error")
    {
      const XmlNode* error = stanza.get_child("error", COMPONENT_NS);
      // Only a set of errors are considered “fatal”. If we encounter one of
      // them, we purge (we disconnect the user from all the IRC servers).
      // We consider this to be true, unless the error condition is
      // specified and is not in the kickable_errors set
      bool kickable_error = true;
      if (error && error->has_children())
        {
          const XmlNode* condition = error->get_last_child();
          if (kickable_errors.find(condition->get_name()) == kickable_errors.end())
            kickable_error = false;
        }
      if (kickable_error)
        bridge->shutdown("Error from remote client");
    }
  else if (type == "chat")
    {
      if (body && !body->get_inner().empty())
        {
          // a message for nick!server
          if (iid.is_user && !iid.get_local().empty())
            {
              bridge->send_private_message(iid, body->get_inner());
              bridge->remove_preferred_from_jid(iid.get_local());
            }
          else if (!iid.is_user && !to.resource.empty())
            { // a message for chan%server@biboumi/Nick or
              // server@biboumi/Nick
              // Convert that into a message to nick!server
              Iid user_iid(utils::tolower(to.resource) + "!" + iid.get_server());
              bridge->send_private_message(user_iid, body->get_inner());
              bridge->set_preferred_from_jid(user_iid.get_local(), to_str);
            }
        }
    }
  else if (iid.is_user)
    this->send_invalid_user_error(to.local, from);
  stanza_error.disable();
}

// We MUST return an iq, whatever happens, except if the type is
// "result".
// To do this, we use a scopeguard. If an exception is raised somewhere, an
// iq of type error "internal-server-error" is sent. If we handle the
// request properly (by calling a function that registers an iq to be sent
// later, or that directly sends an iq), we disable the ScopeGuard. If we
// reach the end of the function without having disabled the scopeguard, we
// send a "feature-not-implemented" iq as a result.  If an other kind of
// error is found (for example the feature is implemented in biboumi, but
// the request is missing some attribute) we can just change the values of
// error_type and error_name and return from the function (without disabling
// the scopeguard); an iq error will be sent
void BiboumiComponent::handle_iq(const Stanza& stanza)
{
  std::string id = stanza.get_tag("id");
  std::string from = stanza.get_tag("from");
  std::string to_str = stanza.get_tag("to");
  std::string type = stanza.get_tag("type");

  if (from.empty())
    return;
  if (id.empty() || to_str.empty() || type.empty())
    {
      this->send_stanza_error("iq", from, this->served_hostname, id,
                              "modify", "bad-request", "");
      return;
    }

  Bridge* bridge = this->get_user_bridge(from);
  Jid to(to_str);

  // These two values will be used in the error iq sent if we don't disable
  // the scopeguard.
  std::string error_type("cancel");
  std::string error_name("internal-server-error");
  utils::ScopeGuard stanza_error([&](){
      this->send_stanza_error("iq", from, to_str, id,
                              error_type, error_name, "");
    });
  if (type == "set")
    {
      XmlNode* query;
      if ((query = stanza.get_child("query", MUC_ADMIN_NS)))
        {
          const XmlNode* child = query->get_child("item", MUC_ADMIN_NS);
          if (child)
            {
              std::string nick = child->get_tag("nick");
              std::string role = child->get_tag("role");
              std::string affiliation = child->get_tag("affiliation");
              if (!nick.empty())
                {
                  Iid iid(to.local);
                  if (role == "none")
                    {               // This is a kick
                      std::string reason;
                      XmlNode* reason_el = child->get_child("reason", MUC_ADMIN_NS);
                      if (reason_el)
                        reason = reason_el->get_inner();
                      bridge->send_irc_kick(iid, nick, reason, id, from);
                    }
                  else
                    bridge->forward_affiliation_role_change(iid, nick, affiliation, role);
                  stanza_error.disable();
                }
            }
        }
      else if ((query = stanza.get_child("command", ADHOC_NS)))
        {
          Stanza response("iq");
          response["to"] = from;
          response["from"] = this->served_hostname;
          response["id"] = id;
          XmlNode inner_node = this->adhoc_commands_handler.handle_request(from, *query);
          if (inner_node.get_child("error", ADHOC_NS))
            response["type"] = "error";
          else
            response["type"] = "result";
          response.add_child(std::move(inner_node));
          response.close();
          this->send_stanza(response);
          stanza_error.disable();
        }
    }
  else if (type == "get")
    {
      XmlNode* query;
      if ((query = stanza.get_child("query", DISCO_INFO_NS)))
        { // Disco info
          if (to_str == this->served_hostname)
            {
              const std::string node = query->get_tag("node");
              if (node.empty())
                {
                  // On the gateway itself
                  this->send_self_disco_info(id, from);
                  stanza_error.disable();
                }
            }
        }
      else if ((query = stanza.get_child("query", VERSION_NS)))
        {
          Iid iid(to.local);
          if (iid.is_user ||
              (iid.is_channel && !to.resource.empty()))
            {
              // Get the IRC user version
              std::string target;
              if (iid.is_user)
                target = iid.get_local();
              else
                target = to.resource;
              bridge->send_irc_version_request(iid.get_server(), target, id,
                                               from, to_str);
            }
          else
            {
              // On the gateway itself or on a channel
              this->send_version(id, from, to_str);
            }
          stanza_error.disable();
        }
      else if ((query = stanza.get_child("query", DISCO_ITEMS_NS)))
        {
          Iid iid(to.local);
          const std::string node = query->get_tag("node");
          if (node == ADHOC_NS)
            {
              this->send_adhoc_commands_list(id, from);
              stanza_error.disable();
            }
          else if (node.empty() && !iid.is_user && !iid.is_channel)
            { // Disco on an IRC server: get the list of channels
              bridge->send_irc_channel_list_request(iid, id, from);
              stanza_error.disable();
            }
        }
      else if ((query = stanza.get_child("ping", PING_NS)))
        {
          Iid iid(to.local);
          if (iid.is_user)
            { // Ping any user (no check on the nick done ourself)
              bridge->send_irc_user_ping_request(iid.get_server(),
                                                 iid.get_local(), id, from, to_str);
            }
          else if (iid.is_channel && !to.resource.empty())
            { // Ping a room participant (we check if the nick is in the room)
              bridge->send_irc_participant_ping_request(iid,
                                                        to.resource, id, from, to_str);
            }
          else
            { // Ping a channel, a server or the gateway itself
              bridge->on_gateway_ping(iid.get_server(),
                                     id, from, to_str);
            }
          stanza_error.disable();
        }
    }
  else if (type == "result")
    {
      stanza_error.disable();
      XmlNode* query;
      if ((query = stanza.get_child("query", VERSION_NS)))
        {
          XmlNode* name_node = query->get_child("name", VERSION_NS);
          XmlNode* version_node = query->get_child("version", VERSION_NS);
          XmlNode* os_node = query->get_child("os", VERSION_NS);
          std::string name;
          std::string version;
          std::string os;
          if (name_node)
            name = name_node->get_inner() + " (through the biboumi gateway)";
          if (version_node)
            version = version_node->get_inner();
          if (os_node)
            os = os_node->get_inner();
          const Iid iid(to.local);
          bridge->send_xmpp_version_to_irc(iid, name, version, os);
        }
      else
        {
          const auto it = this->waiting_iq.find(id);
          if (it != this->waiting_iq.end())
            {
              it->second(bridge, stanza);
              this->waiting_iq.erase(it);
            }
        }
    }
  error_type = "cancel";
  error_name = "feature-not-implemented";
}

Bridge* BiboumiComponent::get_user_bridge(const std::string& user_jid)
{
  try
    {
      return this->bridges.at(user_jid).get();
    }
  catch (const std::out_of_range& exception)
    {
      this->bridges.emplace(user_jid, std::make_unique<Bridge>(user_jid, this, this->poller));
      return this->bridges.at(user_jid).get();
    }
}

Bridge* BiboumiComponent::find_user_bridge(const std::string& user_jid)
{
  try
    {
      return this->bridges.at(user_jid).get();
    }
  catch (const std::out_of_range& exception)
    {
      return nullptr;
    }
}

std::list<Bridge*> BiboumiComponent::get_bridges() const
{
  std::list<Bridge*> res;
  for (auto it = this->bridges.begin(); it != this->bridges.end(); ++it)
    res.push_back(it->second.get());
  return res;
}

void BiboumiComponent::send_self_disco_info(const std::string& id, const std::string& jid_to)
{
  Stanza iq("iq");
  iq["type"] = "result";
  iq["id"] = id;
  iq["to"] = jid_to;
  iq["from"] = this->served_hostname;
  XmlNode query("query");
  query["xmlns"] = DISCO_INFO_NS;
  XmlNode identity("identity");
  identity["category"] = "conference";
  identity["type"] = "irc";
  identity["name"] = "Biboumi XMPP-IRC gateway";
  identity.close();
  query.add_child(std::move(identity));
  for (const std::string& ns: {DISCO_INFO_NS, MUC_NS, ADHOC_NS})
    {
      XmlNode feature("feature");
      feature["var"] = ns;
      feature.close();
      query.add_child(std::move(feature));
    }
  query.close();
  iq.add_child(std::move(query));
  iq.close();
  this->send_stanza(iq);
}

void BiboumiComponent::send_adhoc_commands_list(const std::string& id, const std::string& requester_jid)
{
  Stanza iq("iq");
  iq["type"] = "result";
  iq["id"] = id;
  iq["to"] = requester_jid;
  iq["from"] = this->served_hostname;
  XmlNode query("query");
  query["xmlns"] = DISCO_ITEMS_NS;
  query["node"] = ADHOC_NS;
  for (const auto& kv: this->adhoc_commands_handler.get_commands())
    {
      XmlNode item("item");
      item["jid"] = this->served_hostname;
      item["node"] = kv.first;
      item["name"] = kv.second.name;
      item.close();
      query.add_child(std::move(item));
    }
  query.close();
  iq.add_child(std::move(query));
  iq.close();
  this->send_stanza(iq);
}

void BiboumiComponent::send_iq_version_request(const std::string& from,
                                            const std::string& jid_to)
{
  Stanza iq("iq");
  iq["type"] = "get";
  iq["id"] = "version_"s + this->next_id();
  iq["from"] = from + "@" + this->served_hostname;
  iq["to"] = jid_to;
  XmlNode query("query");
  query["xmlns"] = VERSION_NS;
  query.close();
  iq.add_child(std::move(query));
  iq.close();
  this->send_stanza(iq);
}

void BiboumiComponent::send_ping_request(const std::string& from,
                                      const std::string& jid_to,
                                      const std::string& id)
{
  Stanza iq("iq");
  iq["type"] = "get";
  iq["id"] = id;
  iq["from"] = from + "@" + this->served_hostname;
  iq["to"] = jid_to;
  XmlNode ping("ping");
  ping["xmlns"] = PING_NS;
  ping.close();
  iq.add_child(std::move(ping));
  iq.close();
  this->send_stanza(iq);

  auto result_cb = [from, id](Bridge* bridge, const Stanza& stanza)
    {
      Jid to(stanza.get_tag("to"));
      if (to.local != from)
        {
          log_error("Received a corresponding ping result, but the 'to' from "
                    "the response mismatches the 'from' of the request");
        }
      else
        bridge->send_irc_ping_result(from, id);
    };
  this->waiting_iq[id] = result_cb;
}

void BiboumiComponent::send_iq_room_list_result(const std::string& id,
                                             const std::string to_jid,
                                             const std::string& from,
                                             const std::vector<ListElement>& rooms_list)
{
  Stanza iq("iq");
  iq["from"] = from + "@" + this->served_hostname;
  iq["to"] = to_jid;
  iq["id"] = id;
  iq["type"] = "result";
  XmlNode query("query");
  query["xmlns"] = DISCO_ITEMS_NS;
  for (const auto& room: rooms_list)
    {
      XmlNode item("item");
      item["jid"] = room.channel + "%" + from + "@" + this->served_hostname;
      item.close();
      query.add_child(std::move(item));
    }
  query.close();
  iq.add_child(std::move(query));
  iq.close();
  this->send_stanza(iq);
}
