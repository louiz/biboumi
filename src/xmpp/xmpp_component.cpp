#include <utils/make_unique.hpp>
#include <utils/scopeguard.hpp>
#include <logger/logger.hpp>

#include <xmpp/xmpp_component.hpp>
#include <xmpp/jid.hpp>

#include <utils/sha1.hpp>

#include <stdexcept>
#include <iostream>

#include <stdio.h>

#define STREAM_NS        "http://etherx.jabber.org/streams"
#define COMPONENT_NS     "jabber:component:accept"
#define MUC_NS           "http://jabber.org/protocol/muc"
#define MUC_USER_NS      MUC_NS"#user"
#define MUC_ADMIN_NS     MUC_NS"#admin"
#define DISCO_NS         "http://jabber.org/protocol/disco"
#define DISCO_ITEMS_NS   DISCO_NS"#items"
#define DISCO_INFO_NS    DISCO_NS"#info"
#define XHTMLIM_NS       "http://jabber.org/protocol/xhtml-im"
#define STANZA_NS        "urn:ietf:params:xml:ns:xmpp-stanzas"
#define STREAMS_NS       "urn:ietf:params:xml:ns:xmpp-streams"

XmppComponent::XmppComponent(const std::string& hostname, const std::string& secret):
  ever_auth(false),
  last_auth(false),
  served_hostname(hostname),
  secret(secret),
  authenticated(false),
  doc_open(false)
{
  this->parser.add_stream_open_callback(std::bind(&XmppComponent::on_remote_stream_open, this,
                                                  std::placeholders::_1));
  this->parser.add_stanza_callback(std::bind(&XmppComponent::on_stanza, this,
                                                  std::placeholders::_1));
  this->parser.add_stream_close_callback(std::bind(&XmppComponent::on_remote_stream_close, this,
                                                  std::placeholders::_1));
  this->stanza_handlers.emplace(COMPONENT_NS":handshake",
                                std::bind(&XmppComponent::handle_handshake, this,std::placeholders::_1));
  this->stanza_handlers.emplace(COMPONENT_NS":presence",
                                std::bind(&XmppComponent::handle_presence, this,std::placeholders::_1));
  this->stanza_handlers.emplace(COMPONENT_NS":message",
                                std::bind(&XmppComponent::handle_message, this,std::placeholders::_1));
  this->stanza_handlers.emplace(COMPONENT_NS":iq",
                                std::bind(&XmppComponent::handle_iq, this,std::placeholders::_1));
  this->stanza_handlers.emplace(STREAM_NS":error",
                                std::bind(&XmppComponent::handle_error, this,std::placeholders::_1));
}

XmppComponent::~XmppComponent()
{
}

void XmppComponent::start()
{
  this->connect("127.0.0.1", "5347");
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
  log_error("Failed to connect to the XMPP server: " << reason);
}

void XmppComponent::on_connected()
{
  log_info("connected to XMPP server");
  XmlNode node("stream:stream", nullptr);
  node["xmlns"] = COMPONENT_NS;
  node["xmlns:stream"] = STREAM_NS;
  node["to"] = this->served_hostname;
  this->send_stanza(node);
  this->doc_open = true;
  // We may have some pending data to send: this happens when we try to send
  // some data before we are actually connected.  We send that data right now, if any
  this->send_pending_data();
}

void XmppComponent::on_connection_close()
{
  log_info("XMPP server closed connection");
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
    it->second->shutdown();
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

  this->last_auth = false;
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

  Stanza handshake("handshake");
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
                       const std::string& defined_condition, const std::string& text)
{
  Stanza node(kind);
  if (!to.empty())
    node["to"] = to;
  if (!from.empty())
    node["from"] = from;
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
  this->last_auth = true;
  log_info("Authenticated with the XMPP server");
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

  if (!iid.chan.empty() && !iid.server.empty())
    { // presence toward a MUC that corresponds to an irc channel
      if (type.empty())
        {
          const std::string own_nick = bridge->get_own_nick(iid);
          if (!own_nick.empty() && own_nick != to.resource)
            bridge->send_irc_nick_change(iid, to.resource);
          bridge->join_irc_channel(iid, to.resource);
        }
      else if (type == "unavailable")
        {
          XmlNode* status = stanza.get_child(COMPONENT_NS":status");
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
  XmlNode* body = stanza.get_child(COMPONENT_NS":body");
  if (type == "groupchat")
    {
      if (to.resource.empty())
        if (body && !body->get_inner().empty())
          bridge->send_channel_message(iid, body->get_inner());
      XmlNode* subject = stanza.get_child(COMPONENT_NS":subject");
      if (subject)
        bridge->set_channel_topic(iid, subject->get_inner());
    }
  else
    {
      if (body && !body->get_inner().empty())
        bridge->send_private_message(iid, body->get_inner());
    }
  stanza_error.disable();
}

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
  Jid to(from);

  std::string error_type("cancel");
  std::string error_name("internal-server-error");
  utils::ScopeGuard stanza_error([&](){
      this->send_stanza_error("iq", from, to_str, id,
                              error_type, error_name, "");
        });
  if (type == "set")
    {
      XmlNode* query;
      if ((query = stanza.get_child(MUC_ADMIN_NS":query")))
        {
          const XmlNode* child = query->get_child(MUC_ADMIN_NS":item");
          if (child)
            {
              std::string nick = child->get_tag("nick");
              std::string role = child->get_tag("role");
              if (!nick.empty() && role == "none")
                {
                  std::string reason;
                  XmlNode* reason_el = child->get_child(MUC_ADMIN_NS":reason");
                  if (reason_el)
                    reason = reason_el->get_inner();
                  Iid iid(to.local);
                  bridge->send_irc_kick(iid, nick, reason);
                }
              else
                {
                  error_type = "cancel";
                  error_name = "feature-not-implemented";
                  return;
                }
            }
        }
    }
  else
    {
      error_type = "cancel";
      error_name = "feature-not-implemented";
      return;
    }
  stanza_error.disable();
}

void XmppComponent::handle_error(const Stanza& stanza)
{
  XmlNode* text = stanza.get_child(STREAMS_NS":text");
  std::string error_message("Unspecified error");
  if (text)
    error_message = text->get_inner();
  log_error("Stream error received from the XMPP server: " << error_message);
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

void* XmppComponent::get_receive_buffer(const size_t size) const
{
  return this->parser.get_buffer(size);
}

void XmppComponent::send_message(const std::string& from, Xmpp::body&& body, const std::string& to, const std::string& type)
{
  XmlNode node("message");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname;
  if (!type.empty())
    node["type"] = type;
  XmlNode body_node("body");
  body_node.set_inner(std::get<0>(body));
  body_node.close();
  node.add_child(std::move(body_node));
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
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
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

void XmppComponent::send_muc_leave(std::string&& muc_name, std::string&& nick, Xmpp::body&& message, const std::string& jid_to, const bool self)
{
  Stanza presence("presence");
  presence["to"] = jid_to;
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  presence["type"] = "unavailable";
  const std::string message_str = std::get<0>(message);
  if (!message_str.empty() || self)
    {
      XmlNode status("status");
      if (!message_str.empty())
        status.set_inner(message_str);
      if (self)
        status["code"] = "110";
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

void XmppComponent::send_nickname_conflict_error(const std::string& muc_name,
                                                 const std::string& nickname,
                                                 const std::string& jid_to)
{
  Stanza presence("presence");
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + nickname;
  presence["to"] = jid_to;
  XmlNode x("x");
  x["xmlns"] = MUC_NS;
  x.close();
  presence.add_child(std::move(x));
  XmlNode error("error");
  error["by"] = muc_name + "@" + this->served_hostname;
  error["type"] = "cancel";
  error["code"] = "409";
  XmlNode conflict("conflict");
  conflict["xmlns"] = STANZA_NS;
  conflict.close();
  error.add_child(std::move(conflict));
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
