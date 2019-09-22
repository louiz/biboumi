#include <utils/timed_events.hpp>
#include <utils/scopeguard.hpp>
#include <utils/tolower.hpp>
#include <logger/logger.hpp>
#include <utils/uuid.hpp>

#include <xmpp/xmpp_component.hpp>
#include <config/config.hpp>
#include <utils/system.hpp>
#include <utils/time.hpp>
#include <xmpp/auth.hpp>
#include <xmpp/jid.hpp>

#include <stdexcept>
#include <iostream>
#include <set>

#include <cstdlib>
#include <set>

#include <biboumi.h>
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

XmppComponent::XmppComponent(std::shared_ptr<Poller>& poller, std::string hostname, std::string secret):
  TCPClientSocketHandler(poller),
  ever_auth(false),
  first_connection_try(true),
  secret(std::move(secret)),
  authenticated(false),
  doc_open(false),
  served_hostname(std::move(hostname)),
  stanza_handlers{},
  adhoc_commands_handler(*this)
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
  log_debug("XMPP SENDING: ", str);
  this->send_data(std::move(str));
}

void XmppComponent::on_connection_failed(const std::string& reason)
{
  this->first_connection_try = false;
  log_error("Failed to connect to the XMPP server: ", reason);
#ifdef SYSTEMD_FOUND
  sd_notifyf(0, "STATUS=Failed to connect to the XMPP server: %s", reason.data());
#endif
}

void XmppComponent::on_connected()
{
  log_info("connected to XMPP server");
  this->first_connection_try = true;
  auto data = "<stream:stream to='" + this->served_hostname + \
    "' xmlns:stream='http://etherx.jabber.org/streams' xmlns='" COMPONENT_NS "'>";
  log_debug("XMPP SENDING: ", data);
  this->send_data(std::move(data));
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
    log_info("XMPP server closed connection: ", error);
}

void XmppComponent::parse_in_buffer(const size_t size)
{
  // in_buf.size, or size, cannot be bigger than our read-size (4096) so it’s safe
  // to cast.

  if (!this->in_buf.empty())
    { // This may happen if the parser could not allocate enough space for
      // us. We try to feed it the data that was read into our in_buf
      // instead. If this fails again we are in trouble.
      this->parser.feed(this->in_buf.data(), static_cast<int>(this->in_buf.size()), false);
      this->in_buf.clear();
    }
  else
    { // Just tell the parser to parse the data that was placed into the
      // buffer it provided to us with GetBuffer
      this->parser.parse(static_cast<int>(size), false);
    }
}

void XmppComponent::on_remote_stream_open(const XmlNode& node)
{
  log_debug("XMPP RECEIVING: ", node.to_string());
  this->stream_id = node.get_tag("id");
  if (this->stream_id.empty())
    {
      log_error("Error: no attribute 'id' found");
      this->send_stream_error("bad-format", "missing 'id' attribute");
      this->close_document();
      return ;
    }

  // Try to authenticate
  auto data = "<handshake xmlns='" COMPONENT_NS "'>" + get_handshake_digest(this->stream_id, this->secret) + "</handshake>";
  log_debug("XMPP SENDING: ", data);
  this->send_data(std::move(data));
}

void XmppComponent::on_remote_stream_close(const XmlNode& node)
{
  log_debug("XMPP RECEIVING: ", node.to_string());
  this->doc_open = false;
}

void XmppComponent::reset()
{
  this->parser.reset();
}

void XmppComponent::on_stanza(const Stanza& stanza)
{
  log_debug("XMPP RECEIVING: ", stanza.to_string());
  std::function<void(const Stanza&)> handler;
  try
    {
      handler = this->stanza_handlers.at(stanza.get_name());
    }
  catch (const std::out_of_range& exception)
    {
      log_warning("No handler for stanza of type ", stanza.get_name());
      return;
    }
  handler(stanza);
}

void XmppComponent::send_stream_error(const std::string& name, const std::string& explanation)
{
  Stanza node("stream:error");
  {
    XmlSubNode error(node, name);
    error["xmlns"] = STREAM_NS;
    if (!explanation.empty())
      error.set_inner(explanation);
  }
  this->send_stanza(node);
}

void XmppComponent::send_stanza_error(const std::string& kind, const std::string& to, const std::string& from,
                                      const std::string& id, const std::string& error_type,
                                      const std::string& defined_condition, const std::string& text,
                                      const bool fulljid)
{
  Stanza node(kind);
  {
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
    {
      XmlSubNode error(node, "error");
      error["type"] = error_type;
      {
        XmlSubNode inner_error(error, defined_condition);
        inner_error["xmlns"] = STANZA_NS;
      }
      if (!text.empty())
        {
          XmlSubNode text_node(error, "text");
          text_node["xmlns"] = STANZA_NS;
          text_node.set_inner(text);
        }
    }
  }
  this->send_stanza(node);
}

void XmppComponent::close_document()
{
  log_debug("XMPP SENDING: </stream:stream>");
  this->send_data("</stream:stream>");
  this->doc_open = false;
}

void XmppComponent::handle_handshake(const Stanza&)
{
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
  log_error("Stream error received from the XMPP server: ", error_message);
#ifdef SYSTEMD_FOUND
  if (!this->ever_auth)
    sd_notifyf(0, "STATUS=Failed to authenticate to the XMPP server: %s", error_message.data());
#endif
}

void* XmppComponent::get_receive_buffer(const size_t size) const
{
  return this->parser.get_buffer(size);
}

void XmppComponent::send_message(const std::string& from, Xmpp::body&& body, const std::string& to,
                                 const std::string& type, const bool fulljid, const bool nocopy,
                                 const bool muc_private)
{
  Stanza message("message");
  {
    message["to"] = to;
    if (fulljid)
      message["from"] = from;
    else
      {
        if (!from.empty())
          message["from"] = from + "@" + this->served_hostname;
        else
          message["from"] = this->served_hostname;
      }
    if (!type.empty())
      message["type"] = type;
    XmlSubNode body_node(message, "body");
    body_node.set_inner(std::get<0>(body));
    if (std::get<1>(body))
      {
        XmlSubNode html(message, "html");
        html["xmlns"] = XHTMLIM_NS;
        // Pass the ownership of the pointer to this xmlnode
        html.add_child(std::move(std::get<1>(body)));
      }
    if (nocopy)
      {
        XmlSubNode private_node(message, "private");
        private_node["xmlns"] = "urn:xmpp:carbons:2";
        XmlSubNode nocopy(message, "no-copy");
        nocopy["xmlns"] = "urn:xmpp:hints";
      }
    if (muc_private)
      {
        XmlSubNode x(message, "x");
        x["xmlns"] = MUC_USER_NS;
      }
  }
  this->send_stanza(message);
}

void XmppComponent::send_user_join(const std::string& from,
                                   const std::string& nick,
                                   const std::string& realjid,
                                   const std::string& affiliation,
                                   const std::string& role,
                                   const std::string& to,
                                   const bool self)
{
  Stanza presence("presence");
  {
    presence["to"] = to;
    presence["from"] = from + "@" + this->served_hostname + "/" + nick;

    XmlSubNode x(presence, "x");
    x["xmlns"] = MUC_USER_NS;

    XmlSubNode item(x, "item");
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

    if (self)
      {
        XmlSubNode status_self(x, "status");
        status_self["code"] = "110";
        XmlSubNode status_nick_modified(x, "status");
        status_nick_modified["code"] = "210";
      }
  }
  this->send_stanza(presence);
}

void XmppComponent::send_topic(const std::string& from, Xmpp::body&& topic, const std::string& to, const std::string& who)
{
  Stanza message("message");
  {
    message["to"] = to;
    if (who.empty())
      message["from"] = from + "@" + this->served_hostname;
    else
      message["from"] = from + "@" + this->served_hostname + "/" + who;
    message["type"] = "groupchat";
    XmlSubNode subject(message, "subject");
    subject.set_inner(std::get<0>(topic));
  }
  this->send_stanza(message);
}

void XmppComponent::send_muc_message(const std::string& muc_name, const std::string& nick, Xmpp::body&& xmpp_body, const std::string& jid_to, std::string uuid, std::string id)
{
  Stanza message("message");
  message["to"] = jid_to;
  message["id"] = std::move(id);
  if (!nick.empty())
    message["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  else // Message from the room itself
    message["from"] = muc_name + "@" + this->served_hostname;
  message["type"] = "groupchat";

  {
    XmlSubNode body(message, "body");
    body.set_inner(std::get<0>(xmpp_body));
  }

  if (std::get<1>(xmpp_body))
    {
      XmlSubNode html(message, "html");
      html["xmlns"] = XHTMLIM_NS;
      // Pass the ownership of the pointer to this xmlnode
      html.add_child(std::move(std::get<1>(xmpp_body)));
    }

  if (!uuid.empty())
    {
      XmlSubNode stanza_id(message, "stanza-id");
      stanza_id["xmlns"] = STABLE_ID_NS;
      stanza_id["by"] = muc_name + "@" + this->served_hostname;
      stanza_id["id"] = std::move(uuid);
    }

  this->send_stanza(message);
}

#ifdef USE_DATABASE
void XmppComponent::send_history_message(const std::string& muc_name, const std::string& nick, const std::string& body_txt, const std::string& jid_to, Database::time_point::rep timestamp)
{
  Stanza message("message");
  message["to"] = jid_to;
  if (!nick.empty())
    message["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  else
    message["from"] = muc_name + "@" + this->served_hostname;
  message["type"] = "groupchat";

  {
    XmlSubNode body(message, "body");
    body.set_inner(body_txt);
  }
  {
    XmlSubNode delay(message, "delay");
    delay["xmlns"] = DELAY_NS;
    delay["from"] = muc_name + "@" + this->served_hostname;
    delay["stamp"] = utils::to_string(timestamp);
  }

  this->send_stanza(message);
}
#endif

void XmppComponent::send_muc_leave(const std::string& muc_name, const std::string& nick, Xmpp::body&& message,
                                   const std::string& jid_to, const bool self, const bool user_requested,
                                   const std::string& affiliation, const std::string& role)
{
  Stanza presence("presence");
  {
    presence["to"] = jid_to;
    presence["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
    presence["type"] = "unavailable";
    const std::string& message_str = std::get<0>(message);
    XmlSubNode x(presence, "x");
    x["xmlns"] = MUC_USER_NS;
    if (self)
      {
        {
          XmlSubNode status(x, "status");
          status["code"] = "110";
        }
        if (!user_requested)
          {
            XmlSubNode status(x, "status");
            status["code"] = "332";
          }
      }
    XmlSubNode item(x, "item");
    item["affiliation"] = affiliation;
    item["role"] = role;
    if (!message_str.empty())
      {
        XmlSubNode status(presence, "status");
        status.set_inner(message_str);
      }
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
  {
    presence["to"] = jid_to;
    presence["from"] = muc_name + "@" + this->served_hostname + "/" + old_nick;
    presence["type"] = "unavailable";
    XmlSubNode x(presence, "x");
    x["xmlns"] = MUC_USER_NS;
    XmlSubNode item(x, "item");
    item["nick"] = new_nick;
    XmlSubNode status(x, "status");
    status["code"] = "303";
    if (self)
      {
        XmlSubNode status(x, "status");
        status["code"] = "110";
      }
  }
  this->send_stanza(presence);

  this->send_user_join(muc_name, new_nick, "", affiliation, role, jid_to, self);
}

void XmppComponent::kick_user(const std::string& muc_name, const std::string& target, const std::string& txt,
                              const std::string& author, const std::string& jid_to, const bool self)
{
  Stanza presence("presence");
  {
    presence["from"] = muc_name + "@" + this->served_hostname + "/" + target;
    presence["to"] = jid_to;
    presence["type"] = "unavailable";
    XmlSubNode x(presence, "x");
    x["xmlns"] = MUC_USER_NS;
    XmlSubNode item(x, "item");
    item["affiliation"] = "none";
    item["role"] = "none";
    XmlSubNode actor(item, "actor");
    actor["nick"] = author;
    actor["jid"] = author; // backward compatibility with old clients
    XmlSubNode reason(item, "reason");
    reason.set_inner(txt);
    XmlSubNode status(x, "status");
    status["code"] = "307";
    if (self)
      {
        XmlSubNode status(x, "status");
        status["code"] = "110";
      }
  }
  this->send_stanza(presence);
}

void XmppComponent::send_presence_error(const std::string& muc_name,
                                        const std::string& nickname,
                                        const std::string& jid_to,
                                        const std::string& type,
                                        const std::string& condition,
                                        const std::string& error_code,
                                        const std::string& text)
{
  Stanza presence("presence");
  {
    presence["from"] = muc_name + "@" + this->served_hostname + "/" + nickname;
    presence["to"] = jid_to;
    presence["type"] = "error";
    XmlSubNode x(presence, "x");
    x["xmlns"] = MUC_NS;
    XmlSubNode error(presence, "error");
    error["by"] = muc_name + "@" + this->served_hostname;
    error["type"] = type;
    if (!text.empty())
      {
        XmlSubNode text_node(error, "text");
        text_node["xmlns"] = STANZA_NS;
        text_node.set_inner(text);
      }
    if (!error_code.empty())
      error["code"] = error_code;
    XmlSubNode subnode(error, condition);
    subnode["xmlns"] = STANZA_NS;
  }
  this->send_stanza(presence);
}

void XmppComponent::send_affiliation_role_change(const std::string& muc_name,
                                                 const std::string& target,
                                                 const std::string& affiliation,
                                                 const std::string& role,
                                                 const std::string& jid_to)
{
  Stanza presence("presence");
  {
    presence["from"] = muc_name + "@" + this->served_hostname + "/" + target;
    presence["to"] = jid_to;
    XmlSubNode x(presence, "x");
    x["xmlns"] = MUC_USER_NS;
    XmlSubNode item(x, "item");
    item["affiliation"] = affiliation;
    item["role"] = role;
  }
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
  {
    XmlSubNode query(iq, "query");
    query["xmlns"] = VERSION_NS;
    if (version.empty())
      {
        {
          XmlSubNode name(query, "name");
          name.set_inner("biboumi");
        }
        {
          XmlSubNode version(query, "version");
          version.set_inner(SOFTWARE_VERSION);
        }
        {
          XmlSubNode os(query, "os");
          os.set_inner(utils::get_system_name());
        }
    }
    else
    {
      XmlSubNode name(query, "name");
      name.set_inner(version);
    }
  }
  this->send_stanza(iq);
}

void XmppComponent::send_adhoc_commands_list(const std::string& id, const std::string& requester_jid,
                                             const std::string& from_jid,
                                             const bool with_admin_only, const AdhocCommandsHandler& adhoc_handler)
{
  Stanza iq("iq");
  {
    iq["type"] = "result";
    iq["id"] = id;
    iq["to"] = requester_jid;
    iq["from"] = from_jid;
    XmlSubNode query(iq, "query");
    query["xmlns"] = DISCO_ITEMS_NS;
    query["node"] = ADHOC_NS;
    for (const auto &kv: adhoc_handler.get_commands())
      {
        if (kv.second.is_admin_only() && !with_admin_only)
          continue;
        XmlSubNode item(query, "item");
        item["jid"] = from_jid;
        item["node"] = kv.first;
        item["name"] = kv.second.name;
      }
  }
  this->send_stanza(iq);
}

void XmppComponent::send_iq_version_request(const std::string& from,
                                            const std::string& jid_to)
{
  Stanza iq("iq");
  {
    iq["type"] = "get";
    iq["id"] = "version_" + XmppComponent::next_id();
    iq["from"] = from + "@" + this->served_hostname;
    iq["to"] = jid_to;
    XmlSubNode query(iq, "query");
    query["xmlns"] = VERSION_NS;
  }
  this->send_stanza(iq);
}

void XmppComponent::send_iq_result_full_jid(const std::string& id, const std::string& to_jid, const std::string& from_full_jid, std::unique_ptr<XmlNode> inner)
{
  Stanza iq("iq");
  iq["from"] = from_full_jid;
  iq["to"] = to_jid;
  iq["id"] = id;
  iq["type"] = "result";
  if (inner)
    iq.add_child(std::move(inner));
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
  return utils::gen_uuid();
}
