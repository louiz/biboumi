#include <xmpp/biboumi_component.hpp>

#include <utils/timed_events.hpp>
#include <utils/scopeguard.hpp>
#include <utils/tolower.hpp>
#include <logger/logger.hpp>
#include <xmpp/adhoc_command.hpp>
#include <xmpp/biboumi_adhoc_commands.hpp>
#include <bridge/list_element.hpp>
#include <utils/encoding.hpp>
#include <config/config.hpp>
#include <utils/time.hpp>
#include <xmpp/jid.hpp>

#include <stdexcept>
#include <iostream>

#include <cstdlib>

#include <biboumi.h>

#ifdef SYSTEMD_FOUND
# include <systemd/sd-daemon.h>
#endif

#include <database/database.hpp>
#include <bridge/result_set_management.hpp>
#include <bridge/history_limit.hpp>

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


BiboumiComponent::BiboumiComponent(std::shared_ptr<Poller>& poller, const std::string& hostname, const std::string& secret):
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

  this->adhoc_commands_handler.add_command("ping", {{&PingStep1}, "Do a ping", false});
  this->adhoc_commands_handler.add_command("hello", {{&HelloStep1, &HelloStep2}, "Receive a custom greeting", false});
  this->adhoc_commands_handler.add_command("disconnect-user", {{&DisconnectUserStep1, &DisconnectUserStep2}, "Disconnect selected users from the gateway", true});
  this->adhoc_commands_handler.add_command("disconnect-from-irc-server", {{&DisconnectUserFromServerStep1, &DisconnectUserFromServerStep2, &DisconnectUserFromServerStep3}, "Disconnect from the selected IRC servers", false});
  this->adhoc_commands_handler.add_command("reload", {{&Reload}, "Reload biboumi’s configuration", true});

  AdhocCommand get_irc_connection_info{{&GetIrcConnectionInfoStep1}, "Returns various information about your connection to this IRC server.", false};
  if (!Config::get("fixed_irc_server", "").empty())
    this->adhoc_commands_handler.add_command("get-irc-connection-info", get_irc_connection_info);
  else
    this->irc_server_adhoc_commands_handler.add_command("get-irc-connection-info", get_irc_connection_info);

#ifdef USE_DATABASE
  AdhocCommand configure_server_command({&ConfigureIrcServerStep1, &ConfigureIrcServerStep2}, "Configure a few settings for that IRC server", false);
  AdhocCommand configure_global_command({&ConfigureGlobalStep1, &ConfigureGlobalStep2}, "Configure a few settings", false);

  if (!Config::get("fixed_irc_server", "").empty())
    {
      this->adhoc_commands_handler.add_command("server-configure", configure_server_command);
      this->adhoc_commands_handler.add_command("global-configure", configure_global_command);
    }
  else
    {
      this->adhoc_commands_handler.add_command("configure", configure_global_command);
      this->irc_server_adhoc_commands_handler.add_command("configure", configure_server_command);
    }

  this->irc_channel_adhoc_commands_handler.add_command("configure", {{&ConfigureIrcChannelStep1, &ConfigureIrcChannelStep2}, "Configure a few settings for that IRC channel", false});
#endif
}

void BiboumiComponent::shutdown()
{
  for (auto& pair: this->bridges)
    pair.second->shutdown("Gateway shutdown");
#ifdef USE_DATABASE
  for (const Database::RosterItem& roster_item: Database::get_full_roster())
    {
      this->send_presence_to_contact(roster_item.col<Database::LocalJid>(),
                                     roster_item.col<Database::RemoteJid>(),
                                     "unavailable");
    }
#endif
}

void BiboumiComponent::clean()
{
  auto it = std::begin(this->bridges);
  while (it != std::end(this->bridges))
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
  utils::ScopeGuard stanza_error([this, &from_str, &to_str, &id, &error_type, &error_name](){
      this->send_stanza_error("presence", from_str, to_str, id,
                              error_type, error_name, "");
        });

  try {
  if (iid.type == Iid::Type::Channel && !iid.get_server().empty())
    { // presence toward a MUC that corresponds to an irc channel
      if (type.empty())
        {
          const std::string own_nick = bridge->get_own_nick(iid);
          const XmlNode* x = stanza.get_child("x", MUC_NS);
          const IrcClient* irc = bridge->find_irc_client(iid.get_server());
          // if there is no <x/>, this is a presence status update, we don’t care about those
          if (x)
            {
              const XmlNode* password = x->get_child("password", MUC_NS);
              const XmlNode* history = x->get_child("history", MUC_NS);
              HistoryLimit history_limit;
              if (history)
                {
                  const auto seconds = history->get_tag("seconds");
                  if (!seconds.empty())
                    {
                      const auto now = std::chrono::system_clock::now();
                      std::time_t timestamp = std::chrono::system_clock::to_time_t(now);
                      int int_seconds = std::atoi(seconds.data());
                      timestamp -= int_seconds;
                      history_limit.since = utils::to_string(timestamp);
                    }
                  const auto since = history->get_tag("since");
                  if (!since.empty())
                    history_limit.since = since;
                  const auto maxstanzas = history->get_tag("maxstanzas");
                  if (!maxstanzas.empty())
                    history_limit.stanzas = std::atoi(maxstanzas.data());
                  // Ignore any other value, because this is too complex to implement,
                  // so I won’t do it.
                  if (history->get_tag("maxchars") == "0")
                    history_limit.stanzas = 0;
                }
              bridge->join_irc_channel(iid, to.resource, password ? password->get_inner(): "",
                                       from.resource, history_limit);
            }
          else
            {
              if (irc)
                {
                  const auto chan = irc->find_channel(iid.get_local());
                  if (chan && chan->joined)
                    bridge->send_irc_nick_change(iid, to.resource, from.resource);
                  else
                    { // send an error if we are not joined yet, instead of treating it as a join
                      this->send_stanza_error("presence", from_str, to_str, id, "modify", "not-acceptable", "You are not joined to this MUC.");
                    }
                }
            }
        }
      else if (type == "unavailable")
        {
          const XmlNode* status = stanza.get_child("status", COMPONENT_NS);
          bridge->leave_irc_channel(std::move(iid), status ? status->get_inner() : "", from.resource);
        }
    }
  else if (iid.type == Iid::Type::Server || iid.type == Iid::Type::None)
    {
      if (type == "subscribe")
        { // Auto-accept any subscription request for an IRC server
          this->send_presence_to_contact(to_str, from.bare(), "subscribed", id);
          if (iid.type == Iid::Type::None || bridge->find_irc_client(iid.get_server()))
            this->send_presence_to_contact(to_str, from.bare(), "");
          this->send_presence_to_contact(to_str, from.bare(), "subscribe");
#ifdef USE_DATABASE
          if (!Database::has_roster_item(to_str, from.bare()))
            Database::add_roster_item(to_str, from.bare());
#endif
        }
      else if (type == "unsubscribe")
        {
          this->send_presence_to_contact(to_str, from.bare(), "unavailable", id);
          this->send_presence_to_contact(to_str, from.bare(), "unsubscribed");
          this->send_presence_to_contact(to_str, from.bare(), "unsubscribe");
#ifdef USE_DATABASE
          const bool res = Database::has_roster_item(to_str, from.bare());
          if (res)
            Database::delete_roster_item(to_str, from.bare());
#endif
        }
      else if (type == "probe")
        {
          if ((iid.type == Iid::Type::Server && bridge->find_irc_client(iid.get_server()))
              || iid.type == Iid::Type::None)
            {
#ifdef USE_DATABASE
              if (Database::has_roster_item(to_str, from.bare()))
#endif
                this->send_presence_to_contact(to_str, from.bare(), "");
#ifdef USE_DATABASE
              else // rfc 6121 4.3.2.1
                this->send_presence_to_contact(to_str, from.bare(), "unsubscribed");
#endif
            }
        }
      else if (type.empty())
        { // We just receive a presence from someone (as the result of a probe,
          // or a directed presence, or a normal presence change)
          if (iid.type == Iid::Type::None)
            this->send_presence_to_contact(to_str, from.bare(), "");
        }
    }
  else if (iid.type == Iid::Type::User)
    { // Do nothing yet
    }
  }
  catch (const IRCNotConnected& ex)
    {
      if (type != "unavailable")
        this->send_stanza_error("presence", from_str, to_str, id,
                                "cancel", "recipient-unavailable",
                                "Not connected to IRC server " + ex.hostname,
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
  std::string error_text{};
  utils::ScopeGuard stanza_error([this, &from_str, &to_str, &id, &error_type, &error_name, &error_text](){
      this->send_stanza_error("message", from_str, to_str, id,
                              error_type, error_name, error_text);
    });
  const XmlNode* body = stanza.get_child("body", COMPONENT_NS);

  try {                         // catch IRCNotConnected exceptions
  if (type == "groupchat" && iid.type == Iid::Type::Channel)
    {
      if (body && !body->get_inner().empty())
        {
          if (bridge->is_resource_in_chan(iid.to_tuple(), from.resource))
            {
              // Extract some XML nodes that we must include in the
              // reflection (if any), because XMPP says so
              std::vector<XmlNode> nodes_to_reflect;
              const XmlNode* origin_id = stanza.get_child("origin-id", STABLE_ID_NS);
              if (origin_id)
                nodes_to_reflect.push_back(*origin_id);
              const auto own_address = std::to_string(iid) + '@' + this->served_hostname;
              for (const XmlNode* stanza_id: stanza.get_children("stanza-id", STABLE_ID_NS))
                {
                  // Stanza ID generating entities, which encounter a
                  // <stanza-id/> element where the 'by' attribute matches
                  // the 'by' attribute they would otherwise set, MUST
                  // delete that element even if they are not adding their
                  // own stanza ID.
                  if (stanza_id->get_tag("by") != own_address)
                    nodes_to_reflect.push_back(*stanza_id);
                }
              bridge->send_channel_message(iid, body->get_inner(), id, std::move(nodes_to_reflect));
            }
          else
            {
              error_type = "modify";
              error_name = "not-acceptable";
              error_text = "You are not a participant in this room.";
              return;
            }
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
          const auto fixed_irc_server = Config::get("fixed_irc_server", "");
          // a message for nick!server
          if (iid.type == Iid::Type::User && !iid.get_local().empty())
            {
              bridge->send_private_message(iid, body->get_inner());
            }
          else if (iid.type != Iid::Type::User && !to.resource.empty())
            { // a message for chan%server@biboumi/Nick or
              // server@biboumi/Nick
              // Convert that into a message to nick!server
              Iid user_iid(utils::tolower(to.resource), iid.get_server(), Iid::Type::User);
              bridge->send_private_message(user_iid, body->get_inner());
            }
          else if (iid.type == Iid::Type::Server)
            bridge->send_raw_message(iid.get_server(), body->get_inner());
          else if (iid.type == Iid::Type::None && !fixed_irc_server.empty())
            { // Message sent to the server JID
              // Convert the message body into a raw IRC message
              bridge->send_raw_message(fixed_irc_server, body->get_inner());
            }
          else
            {
              this->send_stanza_error("message", from_str, to_str, id,
                                      "cancel", "not-acceptable",
                                      "This is a regular chat rather than a groupchat. To join an IRC channel, join a groupchat instead.");
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
                Jid invited_jid{invite_to};
                if (invited_jid.domain == this->get_served_hostname() || invited_jid.local.empty())
                  bridge->send_irc_invitation(iid, invite_to);
                else
                  this->send_invitation_from_fulljid(std::to_string(iid), invite_to, from_str);
              }
          }
    }
  } catch (const IRCNotConnected& ex)
    {
      this->send_stanza_error("message", from_str, to_str, id,
                              "cancel", "recipient-unavailable",
                              "Not connected to IRC server " + ex.hostname,
                              true);
    }
  stanza_error.disable();
}

// We MUST return an iq, whatever happens, except if the type is
// "result" or "error".
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
  utils::ScopeGuard stanza_error([this, &from, &to_str, &id, &error_type, &error_name](){
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
                    bridge->forward_affiliation_role_change(iid, from, nick, affiliation, role, id);
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
          Iid iid(to.local, {'#', '&'});
          AdhocCommandsHandler* adhoc_handler;
          if (to.local.empty())
            adhoc_handler = &this->adhoc_commands_handler;
          else
          {
            if (iid.type == Iid::Type::Server)
              adhoc_handler = &this->irc_server_adhoc_commands_handler;
            else if (iid.type == Iid::Type::Channel && to.resource.empty())
              adhoc_handler = &this->irc_channel_adhoc_commands_handler;
            else
              {
                error_name = "feature-not-implemented";
                return;
              }
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
          try {
              if (this->handle_mam_request(stanza))
                stanza_error.disable();
            } catch (const Database::RecordNotFound& exc) {
              error_name = "item-not-found";
              return;
            }
        }
      else if ((query = stanza.get_child("query", MUC_OWNER_NS)))
        {
          if (this->handle_room_configuration_form(*query, from, to, id))
            stanza_error.disable();
        }
#endif
    }
  else if (type == "get")
    {
      const XmlNode* query;
      if ((query = stanza.get_child("query", DISCO_INFO_NS)))
        { // Disco info
          Iid iid(to.local, {'#', '&'});
          const std::string node = query->get_tag("node");
          if (to_str == this->served_hostname)
            {
              if (node.empty())
                {
                  // On the gateway itself
                  this->send_self_disco_info(id, from);
                  stanza_error.disable();
                }
            }
          else if (iid.type == Iid::Type::Server)
            {
                if (node.empty())
                {
                    this->send_irc_server_disco_info(id, from, to_str);
                    stanza_error.disable();
                }
            }
          else if (iid.type == Iid::Type::Channel && to.resource.empty())
            {
              if (node.empty())
                {
                  const IrcClient* irc_client = bridge->find_irc_client(iid.get_server());
                  const IrcChannel* irc_channel{};
                  if (irc_client)
                    irc_channel = irc_client->find_channel(iid.get_local());
                  this->send_irc_channel_disco_info(id, from, to_str, irc_channel);
                  stanza_error.disable();
                }
              else if (node == MUC_TRAFFIC_NS)
                {
                  this->send_irc_channel_muc_traffic_info(id, from, to_str);
                  stanza_error.disable();
                }
            }
        }
      else if ((query = stanza.get_child("query", VERSION_NS)))
        {
          Iid iid(to.local, bridge);
          if ((iid.type == Iid::Type::Channel && !to.resource.empty()) ||
              (iid.type == Iid::Type::User))
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
                                                 Config::is_in_list("admin", from_jid.bare()),
                                                 this->adhoc_commands_handler);
                  stanza_error.disable();
                }
              else if (iid.type == Iid::Type::Server)
                {               // Get the server's adhoc commands
                  this->send_adhoc_commands_list(id, from, to_str,
                                                 Config::is_in_list("admin", from_jid.bare()),
                                                 this->irc_server_adhoc_commands_handler);
                  stanza_error.disable();
                }
              else if (iid.type == Iid::Type::Channel && to.resource.empty())
                {               // Get the channel's adhoc commands
                  this->send_adhoc_commands_list(id, from, to_str,
                                                 Config::is_in_list("admin", from_jid.bare()),
                                                 this->irc_channel_adhoc_commands_handler);
                  stanza_error.disable();
                }
              else // “to” is a MUC user, not the room itself
                error_name = "feature-not-implemented";
            }
          else if (node.empty() && iid.type == Iid::Type::Server)
            { // Disco on an IRC server: get the list of channels
              ResultSetInfo rs_info;
              const XmlNode* set_node = query->get_child("set", RSM_NS);
              if (set_node)
                {
                  const XmlNode* after = set_node->get_child("after", RSM_NS);
                  if (after)
                    rs_info.after = after->get_inner();
                  const XmlNode* before = set_node->get_child("before", RSM_NS);
                  if (before)
                    rs_info.before = before->get_inner();
                  const XmlNode* max = set_node->get_child("max", RSM_NS);
                  if (max)
                    rs_info.max = std::atoi(max->get_inner().data());
                }
              if (rs_info.max == -1)
                rs_info.max = 100;
              bridge->send_irc_channel_list_request(iid, id, from, std::move(rs_info));
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
#ifdef USE_DATABASE
      else if ((query = stanza.get_child("query", MUC_OWNER_NS)))
        {
          if (this->handle_room_configuration_form_request(from, to, id))
            stanza_error.disable();
        }
#endif
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
  else if (type == "error")
    {
      stanza_error.disable();
      const auto it = this->waiting_iq.find(id);
      if (it != this->waiting_iq.end())
        {
          it->second(bridge, stanza);
          this->waiting_iq.erase(it);
        }
    }
  }
  catch (const IRCNotConnected& ex)
    {
      this->send_stanza_error("iq", from, to_str, id,
                              "cancel", "recipient-unavailable",
                              "Not connected to IRC server " + ex.hostname,
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

    Iid iid(to.local, {'#', '&'});
    if (query && iid.type == Iid::Type::Channel && to.resource.empty())
      {
        const std::string query_id = query->get_tag("queryid");
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
        const XmlNode* set = query->get_child("set", RSM_NS);
        int limit = -1;
        Id::real_type reference_record_id{Id::unset_value};
        Database::Paging paging_order{Database::Paging::first};
        if (set)
          {
            const XmlNode* max = set->get_child("max", RSM_NS);
            if (max)
              limit = std::atoi(max->get_inner().data());
            const XmlNode* after = set->get_child("after", RSM_NS);
            if (after)
              {
                auto after_record = Database::get_muc_log(from.bare(), iid.get_local(), iid.get_server(),
                                                          after->get_inner(), start, end);
                reference_record_id = after_record.col<Id>();
              }
            const XmlNode* before = set->get_child("before", RSM_NS);
            if (before)
              {
                paging_order = Database::Paging::last;
                if (!before->get_inner().empty())
                  {
                    auto before_record = Database::get_muc_log(from.bare(), iid.get_local(), iid.get_server(), before->get_inner(), start, end);
                    reference_record_id = before_record.col<Id>();
                  }
              }
          }
        // Do not send more than 100 messages, even if the client asked for more,
        // or if it didn’t specify any limit.
        if (limit < 0 || limit > 100)
          limit = 100;
        auto result = Database::get_muc_logs(from.bare(), iid.get_local(), iid.get_server(),
                                            static_cast<std::size_t>(limit),
                                            start, end,
                                            reference_record_id, paging_order);
        bool complete = std::get<bool>(result);
        auto& lines = std::get<1>(result);

        for (const Database::MucLogLine& line: lines)
          {
            if (!line.col<Database::Nick>().empty())
              this->send_archived_message(line, to.full(), from.full(), query_id);
          }
        {
          auto fin_ptr = std::make_unique<XmlNode>("fin");
          {
            XmlNode& fin = *(fin_ptr.get());
            fin["xmlns"] = MAM_NS;
            if (complete)
              fin["complete"] = "true";
            XmlSubNode set(fin, "set");
            set["xmlns"] = RSM_NS;
            if (!lines.empty())
              {
                XmlSubNode first(set, "first");
                first["index"] = "0";
                first.set_inner(lines[0].col<Database::Uuid>());
                XmlSubNode last(set, "last");
                last.set_inner(lines[lines.size() - 1].col<Database::Uuid>());
              }
          }
          this->send_iq_result_full_jid(id, from.full(), to.full(), std::move(fin_ptr));
        }
        return true;
      }
  return false;
}

void BiboumiComponent::send_archived_message(const Database::MucLogLine& log_line, const std::string& from, const std::string& to,
                                             const std::string& queryid)
{
  Stanza message("message");
  {
    message["from"] = from;
    message["to"] = to;

    XmlSubNode result(message, "result");
    result["xmlns"] = MAM_NS;
    if (!queryid.empty())
      result["queryid"] = queryid;
    result["id"] = log_line.col<Database::Uuid>();

    XmlSubNode forwarded(result, "forwarded");
    forwarded["xmlns"] = FORWARD_NS;

    XmlSubNode delay(forwarded, "delay");
    delay["xmlns"] = DELAY_NS;
    delay["stamp"] = utils::to_string(log_line.col<Database::Date>());

    XmlSubNode submessage(forwarded, "message");
    submessage["xmlns"] = CLIENT_NS;
    submessage["from"] = from + "/" + log_line.col<Database::Nick>();
    submessage["type"] = "groupchat";

    XmlSubNode body(submessage, "body");
    body.set_inner(log_line.col<Database::Body>());
  }
  this->send_stanza(message);
}

bool BiboumiComponent::handle_room_configuration_form_request(const std::string& from, const Jid& to, const std::string& id)
{
  Iid iid(to.local, {'#', '&'});

  if (iid.type != Iid::Type::Channel || !to.resource.empty())
    return false;

  Stanza iq("iq");
  {
    iq["from"] = to.full();
    iq["to"] = from;
    iq["id"] = id;
    iq["type"] = "result";
    XmlSubNode query(iq, "query");
    query["xmlns"] = MUC_OWNER_NS;
    Jid requester(from);
    insert_irc_channel_configuration_form(query, requester, to);
  }
  this->send_stanza(iq);
  return true;
}

bool BiboumiComponent::handle_room_configuration_form(const XmlNode& query, const std::string &from, const Jid &to, const std::string &id)
{
  Iid iid(to.local, {'#', '&'});

  if (iid.type != Iid::Type::Channel || !to.resource.empty())
    return false;

  Jid requester(from);
  if (!handle_irc_channel_configuration_form(*this, query, requester, to))
    return false;

  Stanza iq("iq");
  iq["type"] = "result";
  iq["from"] = to.full();
  iq["to"] = from;
  iq["id"] = id;

  this->send_stanza(iq);

  return true;
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
      return this->bridges.emplace(bare_jid, std::make_unique<Bridge>(bare_jid, *this, this->poller)).first->second.get();
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
  for (const auto& bridge: this->bridges)
    res.push_back(bridge.second.get());
  return res;
}

void BiboumiComponent::send_self_disco_info(const std::string& id, const std::string& jid_to)
{
  Stanza iq("iq");
  {
    iq["type"] = "result";
    iq["id"] = id;
    iq["to"] = jid_to;
    iq["from"] = this->served_hostname;
    XmlSubNode query(iq, "query");
    query["xmlns"] = DISCO_INFO_NS;
    XmlSubNode identity(query, "identity");
    identity["category"] = "conference";
    identity["type"] = "irc";
    identity["name"] = "Biboumi XMPP-IRC gateway";
    for (const char *ns: {DISCO_INFO_NS, MUC_NS, ADHOC_NS, PING_NS, MAM_NS, VERSION_NS, STABLE_MUC_ID_NS})
      {
        XmlSubNode feature(query, "feature");
        feature["var"] = ns;
      }
  }
  this->send_stanza(iq);
}

void BiboumiComponent::send_irc_server_disco_info(const std::string& id, const std::string& jid_to, const std::string& jid_from)
{
  Jid from(jid_from);
  Stanza iq("iq");
  {
    iq["type"] = "result";
    iq["id"] = id;
    iq["to"] = jid_to;
    iq["from"] = jid_from;
    XmlSubNode query(iq, "query");
    query["xmlns"] = DISCO_INFO_NS;
    XmlSubNode identity(query, "identity");
    identity["category"] = "conference";
    identity["type"] = "irc";
    identity["name"] = "IRC server " + from.local + " over Biboumi";
    for (const char *ns: {DISCO_INFO_NS, MUC_NS, ADHOC_NS, PING_NS, MAM_NS, VERSION_NS, STABLE_MUC_ID_NS})
      {
        XmlSubNode feature(query, "feature");
        feature["var"] = ns;
      }
  }
  this->send_stanza(iq);
}

void BiboumiComponent::send_irc_channel_muc_traffic_info(const std::string& id, const std::string& jid_to, const std::string& jid_from)
{
  Stanza iq("iq");
  {
    iq["type"] = "result";
    iq["id"] = id;
    iq["from"] = jid_from;
    iq["to"] = jid_to;

    XmlSubNode query(iq, "query");
    query["xmlns"] = DISCO_INFO_NS;
    query["node"] = MUC_TRAFFIC_NS;
    // We drop all “special” traffic (like xhtml-im, chatstates, etc), so
    // don’t include any <feature/>
  }
  this->send_stanza(iq);
}

void BiboumiComponent::send_irc_channel_disco_info(const std::string& id, const std::string& jid_to,
                                                   const std::string& jid_from, const IrcChannel* irc_channel)
{
  Jid from(jid_from);
  Iid iid(from.local, {});
  Stanza iq("iq");
  {
    iq["type"] = "result";
    iq["id"] = id;
    iq["to"] = jid_to;
    iq["from"] = jid_from;
    XmlSubNode query(iq, "query");
    query["xmlns"] = DISCO_INFO_NS;
    XmlSubNode identity(query, "identity");
    identity["category"] = "conference";
    identity["type"] = "irc";
    identity["name"] = ""s + iid.get_local() + " on " + iid.get_server();
    for (const char *ns: {DISCO_INFO_NS, MUC_NS, ADHOC_NS, PING_NS, MAM_NS, VERSION_NS, STABLE_MUC_ID_NS, SELF_PING_FLAG, "muc_nonanonymous", STABLE_ID_NS})
      {
        XmlSubNode feature(query, "feature");
        feature["var"] = ns;
      }

    XmlSubNode x(query, "x");
    x["xmlns"] = DATAFORM_NS;
    x["type"] = "result";
    {
      XmlSubNode field(x, "field");
      field["var"] = "FORM_TYPE";
      field["type"] = "hidden";
      XmlSubNode value(field, "value");
      value.set_inner("http://jabber.org/protocol/muc#roominfo");
    }

    if (irc_channel && irc_channel->joined)
      {
        XmlSubNode field(x, "field");
        field["var"] = "muc#roominfo_occupants";
        field["label"] = "Number of occupants";
        XmlSubNode value(field, "value");
        value.set_inner(std::to_string(irc_channel->get_users().size()));
      }
  }
  this->send_stanza(iq);
}

void BiboumiComponent::send_ping_request(const std::string& from,
                                         const std::string& jid_to,
                                         const std::string& id)
{
  Stanza iq("iq");
  {
    iq["type"] = "get";
    iq["id"] = id;
    iq["from"] = from + "@" + this->served_hostname;
    iq["to"] = jid_to;
    XmlSubNode ping(iq, "ping");
    ping["xmlns"] = PING_NS;
  }
  this->send_stanza(iq);

  auto result_cb = [from, id](Bridge* bridge, const Stanza& stanza)
    {
      Jid to(stanza.get_tag("to"));
      if (to.local != from)
        {
          log_error("Received a corresponding ping result, but the 'to' from "
                    "the response mismatches the 'from' of the request");
          return;
        }
      const std::string type = stanza.get_tag("type");
      const XmlNode* error = stanza.get_child("error", COMPONENT_NS);
      // Check if what we receive is considered a valid response. And yes, those errors are valid responses
      if (type == "result" ||
          (type == "error" && error && (error->get_child("feature-not-implemented", STANZA_NS) ||
                                        error->get_child("service-unavailable", STANZA_NS))))
        bridge->send_irc_ping_result({from, bridge}, id);
    };
  this->waiting_iq[id] = result_cb;
}

void BiboumiComponent::send_iq_room_list_result(const std::string& id, const std::string& to_jid,
                                                const std::string& from, const ChannelList& channel_list,
                                                std::vector<ListElement>::const_iterator begin,
                                                std::vector<ListElement>::const_iterator end,
                                                const ResultSetInfo& rs_info)
{
  Stanza iq("iq");
  {
    iq["from"] = from + "@" + this->served_hostname;
    iq["to"] = to_jid;
    iq["id"] = id;
    iq["type"] = "result";
    XmlSubNode query(iq, "query");
    query["xmlns"] = DISCO_ITEMS_NS;

    for (auto it = begin; it != end; ++it)
      {
        XmlSubNode item(query, "item");
        std::string channel_name = it->channel;
        xep0106::encode(channel_name);
        item["jid"] = channel_name + "@" + this->served_hostname;
      }

    if ((rs_info.max >= 0 || !rs_info.after.empty() || !rs_info.before.empty()))
      {
        XmlSubNode set_node(query, "set");
        set_node["xmlns"] = RSM_NS;

        if (begin != channel_list.channels.cend())
          {
            XmlSubNode first_node(set_node, "first");
            first_node["index"] = std::to_string(std::distance(channel_list.channels.cbegin(), begin));
            first_node.set_inner(begin->channel + "@" + this->served_hostname);
          }
        if (end != channel_list.channels.cbegin())
          {
            XmlSubNode last_node(set_node, "last");
            last_node.set_inner(std::prev(end)->channel + "@" + this->served_hostname);
          }
        if (channel_list.complete)
          {
            XmlSubNode count_node(set_node, "count");
            count_node.set_inner(std::to_string(channel_list.channels.size()));
          }
      }
  }
  this->send_stanza(iq);
}

void BiboumiComponent::send_invitation(const std::string& room_target,
                                       const std::string& jid_to,
                                       const std::string& author_nick)
{
  if (author_nick.empty())
    this->send_invitation_from_fulljid(room_target, jid_to, room_target + "@" + this->served_hostname);
  else
    this->send_invitation_from_fulljid(room_target, jid_to, room_target + "@" + this->served_hostname + "/" + author_nick);
}

void BiboumiComponent::send_invitation_from_fulljid(const std::string& room_target,
                                       const std::string& jid_to,
                                       const std::string& from)
{
  Stanza message("message");
  {
    message["from"] = room_target + "@" + this->served_hostname;
    message["to"] = jid_to;
    XmlSubNode x(message, "x");
    x["xmlns"] = MUC_USER_NS;
    XmlSubNode invite(x, "invite");
    invite["from"] = from;
  }
  this->send_stanza(message);
}

void BiboumiComponent::accept_subscription(const std::string& from, const std::string& to)
{
  Stanza presence("presence");
  presence["from"] = from;
  presence["to"] = to;
  presence["id"] = this->next_id();
  presence["type"] = "subscribed";
  this->send_stanza(presence);
}

void BiboumiComponent::ask_subscription(const std::string& from, const std::string& to)
{
  Stanza presence("presence");
  presence["from"] = from;
  presence["to"] = to;
  presence["id"] = this->next_id();
  presence["type"] = "subscribe";
  this->send_stanza(presence);
}

void BiboumiComponent::send_presence_to_contact(const std::string& from, const std::string& to,
                                                const std::string& type, const std::string& id)
{
  Stanza presence("presence");
  presence["from"] = from;
  presence["to"] = to;
  if (!type.empty())
    presence["type"] = type;
  if (!id.empty())
    presence["id"] = id;
  this->send_stanza(presence);
}

void BiboumiComponent::on_irc_client_connected(const std::string& irc_hostname, const std::string& jid)
{
#ifdef USE_DATABASE
  const auto local_jid = irc_hostname + "@" + this->served_hostname;
  if (Database::has_roster_item(local_jid, jid))
    this->send_presence_to_contact(local_jid, jid, "");
#else
  (void)irc_hostname;
  (void)jid;
#endif
}

void BiboumiComponent::on_irc_client_disconnected(const std::string& irc_hostname, const std::string& jid)
{
#ifdef USE_DATABASE
  const auto local_jid = irc_hostname + "@" + this->served_hostname;
  if (Database::has_roster_item(local_jid, jid))
    this->send_presence_to_contact(irc_hostname + "@" + this->served_hostname, jid, "unavailable");
#else
  (void)irc_hostname;
  (void)jid;
#endif
}

void BiboumiComponent::after_handshake()
{
  XmppComponent::after_handshake();

#ifdef USE_DATABASE
  const auto contacts = Database::get_contact_list(this->get_served_hostname());

  for (const Database::RosterItem& roster_item: contacts)
    {
      const auto remote_jid = roster_item.col<Database::RemoteJid>();
      // In response, we will receive a presence indicating the
      // contact is online, to which we will respond with our own
      // presence.
      // If the contact removed us from their roster while we were
      // offline, we will receive an unsubscribed presence, letting us
      // stay in sync.
      this->send_presence_to_contact(this->get_served_hostname(), remote_jid, "probe");
    }
#endif
}
