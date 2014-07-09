#include <utils/timed_events.hpp>
#include <utils/scopeguard.hpp>
#include <utils/tolower.hpp>
#include <logger/logger.hpp>

#include <xmpp/xmpp_component.hpp>
#include <config/config.hpp>
#include <xmpp/jid.hpp>
#include <utils/sha1.hpp>

#include <stdexcept>
#include <iostream>

#include <stdio.h>

#include <config.h>

#include <uuid.h>

#ifdef SYSTEMDDAEMON_FOUND
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

XmppComponent::XmppComponent(std::shared_ptr<Poller> poller, const std::string& hostname, const std::string& secret):
  TCPSocketHandler(poller),
  ever_auth(false),
  first_connection_try(true),
  served_hostname(hostname),
  secret(secret),
  authenticated(false),
  doc_open(false),
  adhoc_commands_handler(this)
{
  this->parser.add_stream_open_callback(std::bind(&XmppComponent::on_remote_stream_open, this,
                                                  std::placeholders::_1));
  this->parser.add_stanza_callback(std::bind(&XmppComponent::on_stanza, this,
                                                  std::placeholders::_1));
  this->parser.add_stream_close_callback(std::bind(&XmppComponent::on_remote_stream_close, this,
                                                  std::placeholders::_1));
  this->stanza_handlers.emplace("handshake",
                                std::bind(&XmppComponent::handle_handshake, this,std::placeholders::_1));
  this->stanza_handlers.emplace("presence",
                                std::bind(&XmppComponent::handle_presence, this,std::placeholders::_1));
  this->stanza_handlers.emplace("message",
                                std::bind(&XmppComponent::handle_message, this,std::placeholders::_1));
  this->stanza_handlers.emplace("iq",
                                std::bind(&XmppComponent::handle_iq, this,std::placeholders::_1));
  this->stanza_handlers.emplace("error",
                                std::bind(&XmppComponent::handle_error, this,std::placeholders::_1));
}

XmppComponent::~XmppComponent()
{
}

void XmppComponent::start()
{
  this->connect("localhost", Config::get("port", "5347"), false);
}

bool XmppComponent::is_document_open() const
{
  return this->doc_open;
}

void XmppComponent::send_stanza(const Stanza& stanza)
{
  std::string str = stanza.to_string();
  log_debug("XMPP SENDING: " << str);
  this->send_data(std::move(str));
}

void XmppComponent::on_connection_failed(const std::string& reason)
{
  this->first_connection_try = false;
  log_error("Failed to connect to the XMPP server: " << reason);
#ifdef SYSTEMDDAEMON_FOUND
  sd_notifyf(0, "STATUS=Failed to connect to the XMPP server: %s", reason.data());
#endif
}

void XmppComponent::on_connected()
{
  log_info("connected to XMPP server");
  this->first_connection_try = true;
  XmlNode node("", nullptr);
  node.set_name("stream:stream");
  node["xmlns"] = COMPONENT_NS;
  node["xmlns:stream"] = STREAM_NS;
  node["to"] = this->served_hostname;
  this->send_stanza(node);
  this->doc_open = true;
  // We may have some pending data to send: this happens when we try to send
  // some data before we are actually connected.  We send that data right now, if any
  this->send_pending_data();
}

void XmppComponent::on_connection_close(const std::string& error)
{
  if (error.empty())
    {
      log_info("XMPP server closed connection");
    }
  else
    {
      log_info("XMPP server closed connection: " << error);
    }
}

void XmppComponent::parse_in_buffer(const size_t size)
{
  if (!this->in_buf.empty())
    { // This may happen if the parser could not allocate enough space for
      // us. We try to feed it the data that was read into our in_buf
      // instead. If this fails again we are in trouble.
      this->parser.feed(this->in_buf.data(), this->in_buf.size(), false);
      this->in_buf.clear();
    }
  else
    { // Just tell the parser to parse the data that was placed into the
      // buffer it provided to us with GetBuffer
      this->parser.parse(size, false);
    }
}

void XmppComponent::shutdown()
{
  for (auto it = this->bridges.begin(); it != this->bridges.end(); ++it)
  {
    it->second->shutdown("Gateway shutdown");
  }
}

void XmppComponent::clean()
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

void XmppComponent::on_remote_stream_open(const XmlNode& node)
{
  log_debug("XMPP DOCUMENT OPEN: " << node.to_string());
  this->stream_id = node.get_tag("id");
  if (this->stream_id.empty())
    {
      log_error("Error: no attribute 'id' found");
      this->send_stream_error("bad-format", "missing 'id' attribute");
      this->close_document();
      return ;
    }

  // Try to authenticate
  char digest[HASH_LENGTH * 2 + 1];
  sha1nfo sha1;
  sha1_init(&sha1);
  sha1_write(&sha1, this->stream_id.data(), this->stream_id.size());
  sha1_write(&sha1, this->secret.data(),  this->secret.size());
  const uint8_t* result = sha1_result(&sha1);
  for (int i=0; i < HASH_LENGTH; i++)
    sprintf(digest + (i*2), "%02x", result[i]);
  digest[HASH_LENGTH * 2] = '\0';

  Stanza handshake(COMPONENT_NS":handshake");
  handshake.set_inner(digest);
  handshake.close();
  this->send_stanza(handshake);
}

void XmppComponent::on_remote_stream_close(const XmlNode& node)
{
  log_debug("XMPP DOCUMENT CLOSE " << node.to_string());
  this->doc_open = false;
}

void XmppComponent::reset()
{
  this->parser.reset();
}

void XmppComponent::on_stanza(const Stanza& stanza)
{
  log_debug("XMPP RECEIVING: " << stanza.to_string());
  std::function<void(const Stanza&)> handler;
  try
    {
      handler = this->stanza_handlers.at(stanza.get_name());
    }
  catch (const std::out_of_range& exception)
    {
      log_warning("No handler for stanza of type " << stanza.get_name());
      return;
    }
  handler(stanza);
}

void XmppComponent::send_stream_error(const std::string& name, const std::string& explanation)
{
  XmlNode node("stream:error", nullptr);
  XmlNode error(name, nullptr);
  error["xmlns"] = STREAM_NS;
  if (!explanation.empty())
    error.set_inner(explanation);
  error.close();
  node.add_child(std::move(error));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_stanza_error(const std::string& kind, const std::string& to, const std::string& from,
                                      const std::string& id, const std::string& error_type,
                                      const std::string& defined_condition, const std::string& text,
                                      const bool fulljid)
{
  Stanza node(kind);
  if (!to.empty())
    node["to"] = to;
  if (!from.empty())
    {
      if (fulljid)
        node["from"] = from;
      else
        node["from"] = from + "@" + this->served_hostname;
    }
  if (!id.empty())
    node["id"] = id;
  node["type"] = "error";
  XmlNode error("error");
  error["type"] = error_type;
  XmlNode inner_error(defined_condition);
  inner_error["xmlns"] = STANZA_NS;
  inner_error.close();
  error.add_child(std::move(inner_error));
  if (!text.empty())
    {
      XmlNode text_node("text");
      text_node["xmlns"] = STANZA_NS;
      text_node.set_inner(text);
      text_node.close();
      error.add_child(std::move(text_node));
    }
  error.close();
  node.add_child(std::move(error));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::close_document()
{
  log_debug("XMPP SENDING: </stream:stream>");
  this->send_data("</stream:stream>");
  this->doc_open = false;
}

void XmppComponent::handle_handshake(const Stanza& stanza)
{
  (void)stanza;
  this->authenticated = true;
  this->ever_auth = true;
  log_info("Authenticated with the XMPP server");
#ifdef SYSTEMDDAEMON_FOUND
  sd_notify(0, "READY=1");
  // Install an event that sends a keepalive to systemd.  If biboumi crashes
  // or hangs for too long, systemd will restart it.
  uint64_t usec;
  if (sd_watchdog_enabled(0, &usec) > 0)
    {
      TimedEventsManager::instance().add_event(TimedEvent(
             std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(usec / 2)),
             []() { sd_notify(0, "WATCHDOG=1"); }));
    }
#endif
}

void XmppComponent::handle_presence(const Stanza& stanza)
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

void XmppComponent::handle_message(const Stanza& stanza)
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
void XmppComponent::handle_iq(const Stanza& stanza)
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
          const std::string node = query->get_tag("node");
          if (node == ADHOC_NS)
            {
              this->send_adhoc_commands_list(id, from);
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

void XmppComponent::handle_error(const Stanza& stanza)
{
  XmlNode* text = stanza.get_child("text", STREAMS_NS);
  std::string error_message("Unspecified error");
  if (text)
    error_message = text->get_inner();
  log_error("Stream error received from the XMPP server: " << error_message);
#ifdef SYSTEMDDAEMON_FOUND
  if (!this->ever_auth)
    sd_notifyf(0, "STATUS=Failed to authenticate to the XMPP server: %s", error_message.data());
#endif

}

Bridge* XmppComponent::get_user_bridge(const std::string& user_jid)
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

Bridge* XmppComponent::find_user_bridge(const std::string& user_jid)
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

std::list<Bridge*> XmppComponent::get_bridges() const
{
  std::list<Bridge*> res;
  for (auto it = this->bridges.begin(); it != this->bridges.end(); ++it)
    res.push_back(it->second.get());
  return res;
}

void* XmppComponent::get_receive_buffer(const size_t size) const
{
  return this->parser.get_buffer(size);
}

void XmppComponent::send_message(const std::string& from, Xmpp::body&& body, const std::string& to, const std::string& type, const bool fulljid)
{
  XmlNode node("message");
  node["to"] = to;
  if (fulljid)
    node["from"] = from;
  else
    node["from"] = from + "@" + this->served_hostname;
  if (!type.empty())
    node["type"] = type;
  XmlNode body_node("body");
  body_node.set_inner(std::get<0>(body));
  body_node.close();
  node.add_child(std::move(body_node));
  if (std::get<1>(body))
    {
      XmlNode html("html");
      html["xmlns"] = XHTMLIM_NS;
      // Pass the ownership of the pointer to this xmlnode
      html.add_child(std::get<1>(body).release());
      html.close();
      node.add_child(std::move(html));
    }
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_user_join(const std::string& from,
                                   const std::string& nick,
                                   const std::string& realjid,
                                   const std::string& affiliation,
                                   const std::string& role,
                                   const std::string& to,
                                   const bool self)
{
  XmlNode node("presence");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname + "/" + nick;

  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;

  XmlNode item("item");
  if (!affiliation.empty())
    item["affiliation"] = affiliation;
  if (!role.empty())
    item["role"] = role;
  if (!realjid.empty())
    {
      const std::string preped_jid = jidprep(realjid);
      if (!preped_jid.empty())
        item["jid"] = preped_jid;
    }
  item.close();
  x.add_child(std::move(item));

  if (self)
    {
      XmlNode status("status");
      status["code"] = "110";
      status.close();
      x.add_child(std::move(status));
    }
  x.close();
  node.add_child(std::move(x));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_invalid_room_error(const std::string& muc_name,
                                            const std::string& nick,
                                            const std::string& to)
{
  Stanza presence("presence");
  if (!muc_name.empty())
    presence["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  else
    presence["from"] = this->served_hostname;
  presence["to"] = to;
  presence["type"] = "error";
  XmlNode x("x");
  x["xmlns"] = MUC_NS;
  x.close();
  presence.add_child(std::move(x));
  XmlNode error("error");
  error["by"] = muc_name + "@" + this->served_hostname;
  error["type"] = "cancel";
  XmlNode item_not_found("item-not-found");
  item_not_found["xmlns"] = STANZA_NS;
  item_not_found.close();
  error.add_child(std::move(item_not_found));
  XmlNode text("text");
  text["xmlns"] = STANZA_NS;
  text["xml:lang"] = "en";
  text.set_inner(muc_name +
                 " is not a valid IRC channel name. A correct room jid is of the form: #<chan>%<server>@" +
                 this->served_hostname);
  text.close();
  error.add_child(std::move(text));
  error.close();
  presence.add_child(std::move(error));
  presence.close();
  this->send_stanza(presence);
}

void XmppComponent::send_invalid_user_error(const std::string& user_name, const std::string& to)
{
  Stanza message("message");
  message["from"] = user_name + "@" + this->served_hostname;
  message["to"] = to;
  message["type"] = "error";
  XmlNode x("x");
  x["xmlns"] = MUC_NS;
  x.close();
  message.add_child(std::move(x));
  XmlNode error("error");
  error["type"] = "cancel";
  XmlNode item_not_found("item-not-found");
  item_not_found["xmlns"] = STANZA_NS;
  item_not_found.close();
  error.add_child(std::move(item_not_found));
  XmlNode text("text");
  text["xmlns"] = STANZA_NS;
  text["xml:lang"] = "en";
  text.set_inner(user_name +
                 " is not a valid IRC user name. A correct user jid is of the form: <nick>!<server>@" +
                 this->served_hostname);
  text.close();
  error.add_child(std::move(text));
  error.close();
  message.add_child(std::move(error));
  message.close();
  this->send_stanza(message);
}

void XmppComponent::send_topic(const std::string& from, Xmpp::body&& topic, const std::string& to)
{
  XmlNode message("message");
  message["to"] = to;
  message["from"] = from + "@" + this->served_hostname;
  message["type"] = "groupchat";
  XmlNode subject("subject");
  subject.set_inner(std::get<0>(topic));
  subject.close();
  message.add_child(std::move(subject));
  message.close();
  this->send_stanza(message);
}

void XmppComponent::send_muc_message(const std::string& muc_name, const std::string& nick, Xmpp::body&& xmpp_body, const std::string& jid_to)
{
  Stanza message("message");
  message["to"] = jid_to;
  if (!nick.empty())
    message["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  else // Message from the room itself
    message["from"] = muc_name + "@" + this->served_hostname;
  message["type"] = "groupchat";
  XmlNode body("body");
  body.set_inner(std::get<0>(xmpp_body));
  body.close();
  message.add_child(std::move(body));
  if (std::get<1>(xmpp_body))
    {
      XmlNode html("html");
      html["xmlns"] = XHTMLIM_NS;
      // Pass the ownership of the pointer to this xmlnode
      html.add_child(std::get<1>(xmpp_body).release());
      html.close();
      message.add_child(std::move(html));
    }
  message.close();
  this->send_stanza(message);
}

void XmppComponent::send_muc_leave(const std::string& muc_name, std::string&& nick, Xmpp::body&& message, const std::string& jid_to, const bool self)
{
  Stanza presence("presence");
  presence["to"] = jid_to;
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  presence["type"] = "unavailable";
  const std::string message_str = std::get<0>(message);
  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;
  if (self)
    {
      XmlNode status("status");
      status["code"] = "110";
      status.close();
      x.add_child(std::move(status));
    }
  x.close();
  presence.add_child(std::move(x));
  if (!message_str.empty())
    {
      XmlNode status("status");
      status.set_inner(message_str);
      status.close();
      presence.add_child(std::move(status));
    }
  presence.close();
  this->send_stanza(presence);
}

void XmppComponent::send_nick_change(const std::string& muc_name,
                                     const std::string& old_nick,
                                     const std::string& new_nick,
                                     const std::string& affiliation,
                                     const std::string& role,
                                     const std::string& jid_to,
                                     const bool self)
{
  Stanza presence("presence");
  presence["to"] = jid_to;
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + old_nick;
  presence["type"] = "unavailable";
  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;
  XmlNode item("item");
  item["nick"] = new_nick;
  item.close();
  x.add_child(std::move(item));
  XmlNode status("status");
  status["code"] = "303";
  status.close();
  x.add_child(std::move(status));
  if (self)
    {
      XmlNode status2("status");
      status2["code"] = "110";
      status2.close();
      x.add_child(std::move(status2));
    }
  x.close();
  presence.add_child(std::move(x));
  presence.close();
  this->send_stanza(presence);

  this->send_user_join(muc_name, new_nick, "", affiliation, role, jid_to, self);
}

void XmppComponent::kick_user(const std::string& muc_name,
                                  const std::string& target,
                                  const std::string& txt,
                                  const std::string& author,
                                  const std::string& jid_to)
{
  Stanza presence("presence");
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + target;
  presence["to"] = jid_to;
  presence["type"] = "unavailable";
  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;
  XmlNode item("item");
  item["affiliation"] = "none";
  item["role"] = "none";
  XmlNode actor("actor");
  actor["nick"] = author;
  actor["jid"] = author; // backward compatibility with old clients
  actor.close();
  item.add_child(std::move(actor));
  XmlNode reason("reason");
  reason.set_inner(txt);
  reason.close();
  item.add_child(std::move(reason));
  item.close();
  x.add_child(std::move(item));
  XmlNode status("status");
  status["code"] = "307";
  status.close();
  x.add_child(std::move(status));
  x.close();
  presence.add_child(std::move(x));
  presence.close();
  this->send_stanza(presence);
}

void XmppComponent::send_presence_error(const std::string& muc_name,
                                        const std::string& nickname,
                                        const std::string& jid_to,
                                        const std::string& type,
                                        const std::string& condition,
                                        const std::string& error_code,
                                        const std::string& /* text */)
{
  Stanza presence("presence");
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + nickname;
  presence["to"] = jid_to;
  presence["type"] = "error";
  XmlNode x("x");
  x["xmlns"] = MUC_NS;
  x.close();
  presence.add_child(std::move(x));
  XmlNode error("error");
  error["by"] = muc_name + "@" + this->served_hostname;
  error["type"] = type;
  if (!error_code.empty())
    error["code"] = error_code;
  XmlNode subnode(condition);
  subnode["xmlns"] = STANZA_NS;
  subnode.close();
  error.add_child(std::move(subnode));
  error.close();
  presence.add_child(std::move(error));
  presence.close();
  this->send_stanza(presence);
}

void XmppComponent::send_affiliation_role_change(const std::string& muc_name,
                                                 const std::string& target,
                                                 const std::string& affiliation,
                                                 const std::string& role,
                                                 const std::string& jid_to)
{
  Stanza presence("presence");
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + target;
  presence["to"] = jid_to;
  XmlNode x("x");
  x["xmlns"] = MUC_USER_NS;
  XmlNode item("item");
  item["affiliation"] = affiliation;
  item["role"] = role;
  item.close();
  x.add_child(std::move(item));
  x.close();
  presence.add_child(std::move(x));
  presence.close();
  this->send_stanza(presence);
}

void XmppComponent::send_self_disco_info(const std::string& id, const std::string& jid_to)
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

void XmppComponent::send_version(const std::string& id, const std::string& jid_to, const std::string& jid_from,
                                 const std::string& version)
{
  Stanza iq("iq");
  iq["type"] = "result";
  iq["id"] = id;
  iq["to"] = jid_to;
  iq["from"] = jid_from;
  XmlNode query("query");
  query["xmlns"] = VERSION_NS;
  if (version.empty())
    {
      XmlNode name("name");
      name.set_inner("biboumi");
      name.close();
      query.add_child(std::move(name));
      XmlNode version("version");
      version.set_inner(BIBOUMI_VERSION);
      version.close();
      query.add_child(std::move(version));
      XmlNode os("os");
      os.set_inner(SYSTEM_NAME);
      os.close();
      query.add_child(std::move(os));
    }
  else
    {
      XmlNode name("name");
      name.set_inner(version);
      name.close();
      query.add_child(std::move(name));
    }
  query.close();
  iq.add_child(std::move(query));
  iq.close();
  this->send_stanza(iq);
}

void XmppComponent::send_adhoc_commands_list(const std::string& id, const std::string& requester_jid)
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

void XmppComponent::send_iq_version_request(const std::string& from,
                                            const std::string& jid_to)
{
  Stanza iq("iq");
  iq["type"] = "get";
  iq["id"] = "version_"s + XmppComponent::next_id();
  iq["from"] = from + "@" + this->served_hostname;
  iq["to"] = jid_to;
  XmlNode query("query");
  query["xmlns"] = VERSION_NS;
  query.close();
  iq.add_child(std::move(query));
  iq.close();
  this->send_stanza(iq);
}

void XmppComponent::send_ping_request(const std::string& from,
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

void XmppComponent::send_iq_result(const std::string& id, const std::string& to_jid, const std::string& from_local_part)
{
  Stanza iq("iq");
  if (!from_local_part.empty())
    iq["from"] = from_local_part + "@" + this->served_hostname;
  else
    iq["from"] = this->served_hostname;
  iq["to"] = to_jid;
  iq["id"] = id;
  iq["type"] = "result";
  iq.close();
  this->send_stanza(iq);
}

std::string XmppComponent::next_id()
{
  char uuid_str[37];
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_str);
  return uuid_str;
}
