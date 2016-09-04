#include <xmpp/biboumi_component.hpp>

#include <utils/timed_events.hpp>
#include <utils/scopeguard.hpp>
#include <utils/tolower.hpp>
#include <logger/logger.hpp>
#include <xmpp/adhoc_command.hpp>
#include <xmpp/biboumi_adhoc_commands.hpp>
#include <bridge/list_element.hpp>
#include <config/config.hpp>
#include <utils/sha1.hpp>
#include <utils/time.hpp>
#include <xmpp/jid.hpp>

#include <stdexcept>
#include <iostream>

#include <stdio.h>

#include <louloulibs.h>
#include <biboumi.h>

#include <uuid.h>

#ifdef SYSTEMD_FOUND
# include <systemd/sd-daemon.h>
#endif

#include <database/database.hpp>

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
  XmppComponent(poller, hostname, secret),
  irc_server_adhoc_commands_handler(*this),
  irc_channel_adhoc_commands_handler(*this)
{
  this->stanza_handlers.emplace("presence",
                                std::bind(&BiboumiComponent::handle_presence, this,std::placeholders::_1));
  this->stanza_handlers.emplace("message",
                                std::bind(&BiboumiComponent::handle_message, this,std::placeholders::_1));
  this->stanza_handlers.emplace("iq",
                                std::bind(&BiboumiComponent::handle_iq, this,std::placeholders::_1));

  this->adhoc_commands_handler.get_commands() = {
    {"ping", AdhocCommand({&PingStep1}, "Do a ping", false)},
    {"hello", AdhocCommand({&HelloStep1, &HelloStep2}, "Receive a custom greeting", false)},
    {"disconnect-user", AdhocCommand({&DisconnectUserStep1, &DisconnectUserStep2}, "Disconnect selected users from the gateway", true)},
    {"disconnect-from-irc-servers", AdhocCommand({&DisconnectUserFromServerStep1, &DisconnectUserFromServerStep2, &DisconnectUserFromServerStep3}, "Disconnect from the selected IRC servers", false)},
    {"reload", AdhocCommand({&Reload}, "Reload biboumi’s configuration", true)}
  };

#ifdef USE_DATABASE
  AdhocCommand configure_server_command({&ConfigureIrcServerStep1, &ConfigureIrcServerStep2}, "Configure a few settings for that IRC server", false);
  AdhocCommand configure_global_command({&ConfigureGlobalStep1, &ConfigureGlobalStep2}, "Configure a few settings", false);
  if (!Config::get("fixed_irc_server", "").empty())
    this->adhoc_commands_handler.get_commands().emplace(std::make_pair("configure",
                                                                       configure_server_command));
  else
    this->adhoc_commands_handler.get_commands().emplace(std::make_pair("configure",
                                                                       configure_global_command));
#endif

  this->irc_server_adhoc_commands_handler.get_commands() = {
#ifdef USE_DATABASE
    {"configure", configure_server_command},
#endif
  };
  this->irc_channel_adhoc_commands_handler.get_commands() = {
#ifdef USE_DATABASE
    {"configure", AdhocCommand({&ConfigureIrcChannelStep1, &ConfigureIrcChannelStep2}, "Configure a few settings for that IRC channel", false)},
#endif
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
  std::string from_str = stanza.get_tag("from");
  std::string id = stanza.get_tag("id");
  std::string to_str = stanza.get_tag("to");
  std::string type = stanza.get_tag("type");

  // Check for mandatory tags
  if (from_str.empty())
    {
      log_warning("Received an invalid presence stanza: tag 'from' is missing.");
      return;
    }
  if (to_str.empty())
    {
      this->send_stanza_error("presence", from_str, this->served_hostname, id,
                              "modify", "bad-request", "Missing 'to' tag");
      return;
    }

  Bridge* bridge = this->get_user_bridge(from_str);
  Jid to(to_str);
  Jid from(from_str);
  Iid iid(to.local, bridge);

  // An error stanza is sent whenever we exit this function without
  // disabling this scopeguard.  If error_type and error_name are not
  // changed, the error signaled is internal-server-error. Change their
  // value to signal an other kind of error. For example
  // feature-not-implemented, etc.  Any non-error process should reach the
  // stanza_error.disable() call at the end of the function.
  std::string error_type("cancel");
  std::string error_name("internal-server-error");
  utils::ScopeGuard stanza_error([&](){
      this->send_stanza_error("presence", from_str, to_str, id,
                              error_type, error_name, "");
        });

  try {
  if (iid.type == Iid::Type::Channel && !iid.get_server().empty())
    { // presence toward a MUC that corresponds to an irc channel, or a
      // dummy channel if iid.chan is empty
      if (type.empty())
        {
          const std::string own_nick = bridge->get_own_nick(iid);
          if (!own_nick.empty() && own_nick != to.resource)
            bridge->send_irc_nick_change(iid, to.resource);
          const XmlNode* x = stanza.get_child("x", MUC_NS);
          const XmlNode* password = x ? x->get_child("password", MUC_NS): nullptr;
          bridge->join_irc_channel(iid, to.resource, password ? password->get_inner(): "",
                                   from.resource);
        }
      else if (type == "unavailable")
        {
          const XmlNode* status = stanza.get_child("status", COMPONENT_NS);
          bridge->leave_irc_channel(std::move(iid), status ? status->get_inner() : "", from.resource);
        }
    }
  else
    {
      // An user wants to join an invalid IRC channel, return a presence error to him
      if (type.empty())
        this->send_invalid_room_error(to.local, to.resource, from_str);
    }
  }
  catch (const IRCNotConnected& ex)
    {
      this->send_stanza_error("presence", from_str, to_str, id,
                              "cancel", "remote-server-not-found",
                              "Not connected to IRC server "s + ex.hostname,
                              true);
    }
  stanza_error.disable();
}

void BiboumiComponent::handle_message(const Stanza& stanza)
{
  std::string from_str = stanza.get_tag("from");
  std::string id = stanza.get_tag("id");
  std::string to_str = stanza.get_tag("to");
  std::string type = stanza.get_tag("type");

  if (from_str.empty())
    return;
  if (type.empty())
    type = "normal";
  Bridge* bridge = this->get_user_bridge(from_str);
  Jid from(from_str);
  Jid to(to_str);
  Iid iid(to.local, bridge);

  std::string error_type("cancel");
  std::string error_name("internal-server-error");
  utils::ScopeGuard stanza_error([&](){
      this->send_stanza_error("message", from_str, to_str, id,
                              error_type, error_name, "");
    });
  const XmlNode* body = stanza.get_child("body", COMPONENT_NS);

  try {                         // catch IRCNotConnected exceptions
  if (type == "groupchat" && iid.type == Iid::Type::Channel)
    {
      if (body && !body->get_inner().empty())
        {
          bridge->send_channel_message(iid, body->get_inner());
        }
      const XmlNode* subject = stanza.get_child("subject", COMPONENT_NS);
      if (subject)
        bridge->set_channel_topic(iid, subject->get_inner());
    }
  else if (type == "error")
    {
      const XmlNode* error = stanza.get_child("error", COMPONENT_NS);
      // Only a set of errors are considered “fatal”. If we encounter one of
      // them, we purge (we disconnect that resource from all the IRC servers)
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
        bridge->remove_resource(from.resource, "Error from remote client");
    }
  else if (type == "chat")
    {
      if (body && !body->get_inner().empty())
        {
          // a message for nick!server
          if (iid.type == Iid::Type::User && !iid.get_local().empty())
            {
              bridge->send_private_message(iid, body->get_inner());
              bridge->remove_preferred_from_jid(iid.get_local());
            }
          else if (iid.type != Iid::Type::User && !to.resource.empty())
            { // a message for chan%server@biboumi/Nick or
              // server@biboumi/Nick
              // Convert that into a message to nick!server
              Iid user_iid(utils::tolower(to.resource), iid.get_server(), Iid::Type::User);
              bridge->send_private_message(user_iid, body->get_inner());
              bridge->set_preferred_from_jid(user_iid.get_local(), to_str);
            }
          else if (iid.type == Iid::Type::Server)
            { // Message sent to the server JID
              // Convert the message body into a raw IRC message
              bridge->send_raw_message(iid.get_server(), body->get_inner());
            }
        }
    }
  else if (type == "normal" && iid.type == Iid::Type::Channel)
    {
      if (const XmlNode* x = stanza.get_child("x", MUC_USER_NS))
        if (const XmlNode* invite = x->get_child("invite", MUC_USER_NS))
          {
            const auto invite_to = invite->get_tag("to");
            if (!invite_to.empty())
              {
                bridge->send_irc_invitation(iid, invite_to);
              }
          }

    }
  else if (iid.type == Iid::Type::User)
    this->send_invalid_user_error(to.local, from_str);
  } catch (const IRCNotConnected& ex)
    {
      this->send_stanza_error("message", from_str, to_str, id,
                              "cancel", "remote-server-not-found",
                              "Not connected to IRC server "s + ex.hostname,
                              true);
    }
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

  if (from.empty()) {
    log_warning("Received an iq without a 'from'. Ignoring.");
    return;
  }
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
  try {
  if (type == "set")
    {
      const XmlNode* query;
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
                  Iid iid(to.local, {});
                  if (role == "none")
                    {               // This is a kick
                      std::string reason;
                      const XmlNode* reason_el = child->get_child("reason", MUC_ADMIN_NS);
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
          response["from"] = to_str;
          response["id"] = id;

          // Depending on the 'to' jid in the request, we use one adhoc
          // command handler or an other
          Iid iid(to.local, {});
          AdhocCommandsHandler* adhoc_handler;
          if (to.local.empty())
            adhoc_handler = &this->adhoc_commands_handler;
          else
          {
            if (iid.type == Iid::Type::Server)
              adhoc_handler = &this->irc_server_adhoc_commands_handler;
            else
              adhoc_handler = &this->irc_channel_adhoc_commands_handler;
          }
          // Execute the command, if any, and get a result XmlNode that we
          // insert in our response
          XmlNode inner_node = adhoc_handler->handle_request(from, to_str, *query);
          if (inner_node.get_child("error", ADHOC_NS))
            response["type"] = "error";
          else
            response["type"] = "result";
          response.add_child(std::move(inner_node));
          this->send_stanza(response);
          stanza_error.disable();
        }
#ifdef USE_DATABASE
      else if ((query = stanza.get_child("query", MAM_NS)))
        {
          if (this->handle_mam_request(stanza))
            stanza_error.disable();
        }
#endif
    }
  else if (type == "get")
    {
      const XmlNode* query;
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
          Iid iid(to.local, bridge);
          if (iid.type != Iid::Type::Server && !to.resource.empty())
            {
              // Get the IRC user version
              std::string target;
              if (iid.type == Iid::Type::User)
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
          Iid iid(to.local, bridge);
          const std::string node = query->get_tag("node");
          if (node == ADHOC_NS)
            {
              Jid from_jid(from);
              if (to.local.empty())
                {               // Get biboumi's adhoc commands
                  this->send_adhoc_commands_list(id, from, this->served_hostname,
                                                 (Config::get("admin", "") ==
                                                  from_jid.bare()),
                                                 this->adhoc_commands_handler);
                  stanza_error.disable();
                }
              else if (iid.type == Iid::Type::Server)
                {               // Get the server's adhoc commands
                  this->send_adhoc_commands_list(id, from, to_str,
                                                 (Config::get("admin", "") ==
                                                  from_jid.bare()),
                                                 this->irc_server_adhoc_commands_handler);
                  stanza_error.disable();
                }
              else if (iid.type == Iid::Type::Channel)
                {               // Get the channel's adhoc commands
                  this->send_adhoc_commands_list(id, from, to_str,
                                                 (Config::get("admin", "") ==
                                                  from_jid.bare()),
                                                 this->irc_channel_adhoc_commands_handler);
                  stanza_error.disable();
                }
            }
          else if (node.empty() && iid.type == Iid::Type::Server)
            { // Disco on an IRC server: get the list of channels
              bridge->send_irc_channel_list_request(iid, id, from);
              stanza_error.disable();
            }
        }
      else if ((query = stanza.get_child("ping", PING_NS)))
        {
          Iid iid(to.local, bridge);
          if (iid.type == Iid::Type::User)
            { // Ping any user (no check on the nick done ourself)
              bridge->send_irc_user_ping_request(iid.get_server(),
                                                 iid.get_local(), id, from, to_str);
            }
          else if (iid.type == Iid::Type::Channel && !to.resource.empty())
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
      const XmlNode* query;
      if ((query = stanza.get_child("query", VERSION_NS)))
        {
          const XmlNode* name_node = query->get_child("name", VERSION_NS);
          const XmlNode* version_node = query->get_child("version", VERSION_NS);
          const XmlNode* os_node = query->get_child("os", VERSION_NS);
          std::string name;
          std::string version;
          std::string os;
          if (name_node)
            name = name_node->get_inner() + " (through the biboumi gateway)";
          if (version_node)
            version = version_node->get_inner();
          if (os_node)
            os = os_node->get_inner();
          const Iid iid(to.local, bridge);
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
  }
  catch (const IRCNotConnected& ex)
    {
      this->send_stanza_error("iq", from, to_str, id,
                              "cancel", "remote-server-not-found",
                              "Not connected to IRC server "s + ex.hostname,
                              true);
      stanza_error.disable();
      return;
    }
  error_type = "cancel";
  error_name = "feature-not-implemented";
}

#ifdef USE_DATABASE
bool BiboumiComponent::handle_mam_request(const Stanza& stanza)
{
    std::string id = stanza.get_tag("id");
    Jid from(stanza.get_tag("from"));
    Jid to(stanza.get_tag("to"));

    const XmlNode* query = stanza.get_child("query", MAM_NS);
    std::string query_id;
    if (query)
      query_id = query->get_tag("queryid");

    Iid iid(to.local, {'#', '&'});
    if (iid.type == Iid::Type::Channel && to.resource.empty())
      {
        std::string start;
        std::string end;
        const XmlNode* x = query->get_child("x", DATAFORM_NS);
        if (x)
          {
            const XmlNode* value;
            const auto fields = x->get_children("field", DATAFORM_NS);
            for (const auto& field: fields)
              {
                if (field->get_tag("var") == "start")
                  {
                    value = field->get_child("value", DATAFORM_NS);
                    if (value)
                      start = value->get_inner();
                  }
                else if (field->get_tag("var") == "end")
                  {
                    value = field->get_child("value", DATAFORM_NS);
                    if (value)
                      end = value->get_inner();
                  }
              }
          }
        const auto lines = Database::get_muc_logs(from.bare(), iid.get_local(), iid.get_server(), -1, start, end);
        for (const db::MucLogLine& line: lines)
        {
          const auto queryid = query->get_tag("queryid");
          if (!line.nick.value().empty())
            this->send_archived_message(line, to.full(), from.full(), queryid);
        }
        this->send_iq_result_full_jid(id, from.full(), to.full());
        return true;
      }
  return false;
}

void BiboumiComponent::send_archived_message(const db::MucLogLine& log_line, const std::string& from, const std::string& to,
                                             const std::string& queryid)
{
    Stanza message("message");
    message["from"] = from;
    message["to"] = to;

    XmlNode result("result");
    result["xmlns"] = MAM_NS;
    result["queryid"] = queryid;
    result["id"] = log_line.uuid.value();

    XmlNode forwarded("forwarded");
    forwarded["xmlns"] = FORWARD_NS;

    XmlNode delay("delay");
    delay["xmlns"] = DELAY_NS;
    delay["stamp"] = utils::to_string(log_line.date.value().timeStamp());

    forwarded.add_child(std::move(delay));

    XmlNode submessage("message");
    submessage["xmlns"] = CLIENT_NS;
    submessage["from"] = from + "/" + log_line.nick.value();
    submessage["type"] = "groupchat";

    XmlNode body("body");
    body.set_inner(log_line.body.value());
    submessage.add_child(std::move(body));

    forwarded.add_child(std::move(submessage));
    result.add_child(std::move(forwarded));
    message.add_child(std::move(result));

    this->send_stanza(message);
}

#endif

Bridge* BiboumiComponent::get_user_bridge(const std::string& user_jid)
{
  auto bare_jid = Jid{user_jid}.bare();
  try
    {
      return this->bridges.at(bare_jid).get();
    }
  catch (const std::out_of_range& exception)
    {
      this->bridges.emplace(bare_jid, std::make_unique<Bridge>(bare_jid, *this, this->poller));
      return this->bridges.at(bare_jid).get();
    }
}

Bridge* BiboumiComponent::find_user_bridge(const std::string& full_jid)
{
  auto bare_jid = Jid{full_jid}.bare();
  try
    {
      return this->bridges.at(bare_jid).get();
    }
  catch (const std::out_of_range& exception)
    {
      return nullptr;
    }
}

std::vector<Bridge*> BiboumiComponent::get_bridges() const
{
  std::vector<Bridge*> res;
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
  query.add_child(std::move(identity));
  for (const char* ns: {DISCO_INFO_NS, MUC_NS, ADHOC_NS, PING_NS, MAM_NS, VERSION_NS})
    {
      XmlNode feature("feature");
      feature["var"] = ns;
      query.add_child(std::move(feature));
    }
  iq.add_child(std::move(query));
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
  iq.add_child(std::move(query));
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
  iq.add_child(std::move(ping));
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
        bridge->send_irc_ping_result({from, bridge}, id);
    };
  this->waiting_iq[id] = result_cb;
}

void BiboumiComponent::send_iq_room_list_result(const std::string& id,
                                             const std::string& to_jid,
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
      query.add_child(std::move(item));
    }
  iq.add_child(std::move(query));
  this->send_stanza(iq);
}

void BiboumiComponent::send_invitation(const std::string& room_target,
                                       const std::string& jid_to,
                                       const std::string& author_nick)
{
  Stanza message("message");
  message["from"] = room_target + "@" + this->served_hostname;
  message["to"] = jid_to;
  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;
  XmlNode invite("invite");
  invite["from"] = room_target + "@" + this->served_hostname + "/" + author_nick;
  x.add_child(std::move(invite));
  message.add_child(std::move(x));
  this->send_stanza(message);
}
