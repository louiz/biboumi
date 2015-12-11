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
#include <set>

#include <stdio.h>

#include <uuid.h>

#include <louloulibs.h>
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

XmppComponent::XmppComponent(std::shared_ptr<Poller> poller, const std::string& hostname, const std::string& secret):
  TCPSocketHandler(poller),
  ever_auth(false),
  first_connection_try(true),
  secret(secret),
  authenticated(false),
  doc_open(false),
  served_hostname(hostname),
  stanza_handlers{},
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
  this->stanza_handlers.emplace("error",
                                std::bind(&XmppComponent::handle_error, this,std::placeholders::_1));
}

void XmppComponent::start()
{
  this->connect(Config::get("xmpp_server_ip", "127.0.0.1"), Config::get("port", "5347"), false);
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
#ifdef SYSTEMD_FOUND
  sd_notifyf(0, "STATUS=Failed to connect to the XMPP server: %s", reason.data());
#endif
}

void XmppComponent::on_connected()
{
  log_info("connected to XMPP server");
  this->first_connection_try = true;
  this->send_data("<stream:stream to='");
  this->send_data(std::string{this->served_hostname});
  this->send_data("' xmlns:stream='http://etherx.jabber.org/streams' xmlns='" COMPONENT_NS "'>");
  this->doc_open = true;
  // We may have some pending data to send: this happens when we try to send
  // some data before we are actually connected.  We send that data right now, if any
  this->send_pending_data();
}

void XmppComponent::on_connection_close(const std::string& error)
{
  if (error.empty())
    log_info("XMPP server closed connection");
  else
    log_info("XMPP server closed connection: " << error);
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

  this->send_data("<handshake xmlns='" COMPONENT_NS "'>");
  this->send_data(digest);
  this->send_data("</handshake>");
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
  node.add_child(std::move(error));
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
  error.add_child(std::move(inner_error));
  if (!text.empty())
    {
      XmlNode text_node("text");
      text_node["xmlns"] = STANZA_NS;
      text_node.set_inner(text);
      error.add_child(std::move(text_node));
    }
  node.add_child(std::move(error));
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
#ifdef SYSTEMD_FOUND
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
  this->after_handshake();
}

void XmppComponent::handle_error(const Stanza& stanza)
{
  const XmlNode* text = stanza.get_child("text", STREAMS_NS);
  std::string error_message("Unspecified error");
  if (text)
    error_message = text->get_inner();
  log_error("Stream error received from the XMPP server: " << error_message);
#ifdef SYSTEMD_FOUND
  if (!this->ever_auth)
    sd_notifyf(0, "STATUS=Failed to authenticate to the XMPP server: %s", error_message.data());
#endif

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
  node.add_child(std::move(body_node));
  if (std::get<1>(body))
    {
      XmlNode html("html");
      html["xmlns"] = XHTMLIM_NS;
      // Pass the ownership of the pointer to this xmlnode
      html.add_child(std::move(std::get<1>(body)));
      node.add_child(std::move(html));
    }
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
  x.add_child(std::move(item));

  if (self)
    {
      XmlNode status("status");
      status["code"] = "110";
      x.add_child(std::move(status));
    }
  node.add_child(std::move(x));
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
  presence.add_child(std::move(x));
  XmlNode error("error");
  error["by"] = muc_name + "@" + this->served_hostname;
  error["type"] = "cancel";
  XmlNode item_not_found("item-not-found");
  item_not_found["xmlns"] = STANZA_NS;
  error.add_child(std::move(item_not_found));
  XmlNode text("text");
  text["xmlns"] = STANZA_NS;
  text["xml:lang"] = "en";
  text.set_inner(muc_name +
                 " is not a valid IRC channel name. A correct room jid is of the form: #<chan>%<server>@" +
                 this->served_hostname);
  error.add_child(std::move(text));
  presence.add_child(std::move(error));
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
  message.add_child(std::move(x));
  XmlNode error("error");
  error["type"] = "cancel";
  XmlNode item_not_found("item-not-found");
  item_not_found["xmlns"] = STANZA_NS;
  error.add_child(std::move(item_not_found));
  XmlNode text("text");
  text["xmlns"] = STANZA_NS;
  text["xml:lang"] = "en";
  text.set_inner(user_name +
                 " is not a valid IRC user name. A correct user jid is of the form: <nick>!<server>@" +
                 this->served_hostname);
  error.add_child(std::move(text));
  message.add_child(std::move(error));
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
  message.add_child(std::move(subject));
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
  message.add_child(std::move(body));
  if (std::get<1>(xmpp_body))
    {
      XmlNode html("html");
      html["xmlns"] = XHTMLIM_NS;
      // Pass the ownership of the pointer to this xmlnode
      html.add_child(std::move(std::get<1>(xmpp_body)));
      message.add_child(std::move(html));
    }
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
      x.add_child(std::move(status));
    }
  presence.add_child(std::move(x));
  if (!message_str.empty())
    {
      XmlNode status("status");
      status.set_inner(message_str);
      presence.add_child(std::move(status));
    }
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
  x.add_child(std::move(item));
  XmlNode status("status");
  status["code"] = "303";
  x.add_child(std::move(status));
  if (self)
    {
      XmlNode status2("status");
      status2["code"] = "110";
      x.add_child(std::move(status2));
    }
  presence.add_child(std::move(x));
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
  item.add_child(std::move(actor));
  XmlNode reason("reason");
  reason.set_inner(txt);
  item.add_child(std::move(reason));
  x.add_child(std::move(item));
  XmlNode status("status");
  status["code"] = "307";
  x.add_child(std::move(status));
  presence.add_child(std::move(x));
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
  presence.add_child(std::move(x));
  XmlNode error("error");
  error["by"] = muc_name + "@" + this->served_hostname;
  error["type"] = type;
  if (!error_code.empty())
    error["code"] = error_code;
  XmlNode subnode(condition);
  subnode["xmlns"] = STANZA_NS;
  error.add_child(std::move(subnode));
  presence.add_child(std::move(error));
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
  x.add_child(std::move(item));
  presence.add_child(std::move(x));
  this->send_stanza(presence);
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
      query.add_child(std::move(name));
      XmlNode version("version");
      version.set_inner(SOFTWARE_VERSION);
      query.add_child(std::move(version));
      XmlNode os("os");
      os.set_inner(SYSTEM_NAME);
      query.add_child(std::move(os));
    }
  else
    {
      XmlNode name("name");
      name.set_inner(version);
      query.add_child(std::move(name));
    }
  iq.add_child(std::move(query));
  this->send_stanza(iq);
}

void XmppComponent::send_adhoc_commands_list(const std::string& id, const std::string& requester_jid, const std::string& from_jid, const bool with_admin_only, const AdhocCommandsHandler& adhoc_handler)
{
  Stanza iq("iq");
  iq["type"] = "result";
  iq["id"] = id;
  iq["to"] = requester_jid;
  iq["from"] = from_jid;
  XmlNode query("query");
  query["xmlns"] = DISCO_ITEMS_NS;
  query["node"] = ADHOC_NS;
  for (const auto& kv: adhoc_handler.get_commands())
    {
      if (kv.second.is_admin_only() && !with_admin_only)
        continue;
      XmlNode item("item");
      item["jid"] = from_jid;
      item["node"] = kv.first;
      item["name"] = kv.second.name;
      query.add_child(std::move(item));
    }
  iq.add_child(std::move(query));
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
  iq.add_child(std::move(query));
  this->send_stanza(iq);
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
