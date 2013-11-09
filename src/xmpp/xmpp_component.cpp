#include <utils/make_unique.hpp>

#include <xmpp/xmpp_component.hpp>
#include <xmpp/jid.hpp>

#include <iostream>

// CryptoPP
#include <filters.h>
#include <hex.h>
#include <sha.h>

XmppComponent::XmppComponent(const std::string& hostname, const std::string& secret):
  served_hostname(hostname),
  secret(secret),
  authenticated(false)
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
}

XmppComponent::~XmppComponent()
{
}

void XmppComponent::start()
{
  this->connect(this->served_hostname, "5347");
}

void XmppComponent::send_stanza(const Stanza& stanza)
{
  std::cout << "====== Sending ========" << std::endl;
  std::cout << stanza.to_string() << std::endl;
  this->send_data(stanza.to_string());
}

void XmppComponent::on_connected()
{
  std::cout << "connected to XMPP server" << std::endl;
  XmlNode node("stream:stream", nullptr);
  node["xmlns"] = "jabber:component:accept";
  node["xmlns:stream"] = "http://etherx.jabber.org/streams";
  node["to"] = "irc.abricot";
  this->send_stanza(node);
}

void XmppComponent::on_connection_close()
{
  std::cout << "XMPP server closed connection" << std::endl;
}

void XmppComponent::parse_in_buffer()
{
  this->parser.XML_Parse(this->in_buf.data(), this->in_buf.size(), false);
  this->in_buf.clear();
}

void XmppComponent::on_remote_stream_open(const XmlNode& node)
{
  std::cout << "====== DOCUMENT_OPEN =======" << std::endl;
  std::cout << node.to_string() << std::endl;
  try
    {
      this->stream_id = node["id"];
    }
  catch (const AttributeNotFound& e)
    {
      std::cout << "Error: no attribute 'id' found" << std::endl;
      this->send_stream_error("bad-format", "missing 'id' attribute");
      this->close_document();
      return ;
    }

  // Try to authenticate
  CryptoPP::SHA1 sha1;
  std::string digest;
  CryptoPP::StringSource foo(this->stream_id + this->secret, true,
                      new CryptoPP::HashFilter(sha1,
                          new CryptoPP::HexEncoder(
                              new CryptoPP::StringSink(digest), false)));
  Stanza handshake("handshake", nullptr);
  handshake.set_inner(digest);
  handshake.close();
  this->send_stanza(handshake);
}

void XmppComponent::on_remote_stream_close(const XmlNode& node)
{
  std::cout << "====== DOCUMENT_CLOSE =======" << std::endl;
  std::cout << node.to_string() << std::endl;
}

void XmppComponent::on_stanza(const Stanza& stanza)
{
  std::cout << "=========== STANZA ============" << std::endl;
  std::cout << stanza.to_string() << std::endl;
  std::function<void(const Stanza&)> handler;
  try
    {
      handler = this->stanza_handlers.at(stanza.get_name());
    }
  catch (const std::out_of_range& exception)
    {
      std::cout << "No handler for stanza of type " << stanza.get_name() << std::endl;
      return;
    }
  handler(stanza);
}

void XmppComponent::send_stream_error(const std::string& name, const std::string& explanation)
{
  XmlNode node("stream:error", nullptr);
  XmlNode error(name, nullptr);
  error["xmlns"] = "urn:ietf:params:xml:ns:xmpp-streams";
  if (!explanation.empty())
    error.set_inner(explanation);
  error.close();
  node.add_child(std::move(error));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::close_document()
{
  std::cout << "====== Sending ========" << std::endl;
  std::cout << "</stream:stream>" << std::endl;
  this->send_data("</stream:stream>");
}

void XmppComponent::handle_handshake(const Stanza& stanza)
{
  (void)stanza;
  this->authenticated = true;
}

void XmppComponent::handle_presence(const Stanza& stanza)
{
  Bridge* bridge = this->get_user_bridge(stanza["from"]);
  Jid to(stanza["to"]);
  Iid iid(to.local);
  std::string type;
  try {
    type = stanza["type"];
  }
  catch (const AttributeNotFound&) {}

  if (!iid.chan.empty() && !iid.chan.empty())
    { // presence toward a MUC that corresponds to an irc channel
      if (type.empty())
        bridge->join_irc_channel(iid, to.resource);
      else if (type == "unavailable")
        {
          XmlNode* status = stanza.get_child("status");
          bridge->leave_irc_channel(std::move(iid), status ? std::move(status->get_inner()) : "");
        }
    }
}

void XmppComponent::handle_message(const Stanza& stanza)
{
  Bridge* bridge = this->get_user_bridge(stanza["from"]);
  Jid to(stanza["to"]);
  Iid iid(to.local);
  XmlNode* body = stanza.get_child("body");
  if (stanza["type"] == "groupchat")
    {
      if (to.resource.empty())
        if (body && !body->get_inner().empty())
          bridge->send_channel_message(iid, body->get_inner());
    }
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

void XmppComponent::send_message(const std::string& from, const std::string& body, const std::string& to)
{
  XmlNode node("message");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname;
  XmlNode body_node("body");
  body_node.set_inner(body);
  body_node.close();
  node.add_child(std::move(body_node));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_user_join(const std::string& from, const std::string& nick, const std::string& to)
{
  XmlNode node("presence");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname + "/" + nick;

  XmlNode x("x");
  x["xmlns"] = "http://jabber.org/protocol/muc#user";

  // TODO: put real values here
  XmlNode item("item");
  item["affiliation"] = "member";
  item["role"] = "participant";
  item.close();
  x.add_child(std::move(item));
  x.close();
  node.add_child(std::move(x));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_self_join(const std::string& from, const std::string& nick, const std::string& to)
{
  XmlNode node("presence");
  node["to"] = to;
  node["from"] = from + "@" + this->served_hostname + "/" + nick;

  XmlNode x("x");
  x["xmlns"] = "http://jabber.org/protocol/muc#user";

  // TODO: put real values here
  XmlNode item("item");
  item["affiliation"] = "member";
  item["role"] = "participant";
  item.close();
  x.add_child(std::move(item));

  XmlNode status("status");
  status["code"] = "110";
  status.close();
  x.add_child(std::move(status));

  x.close();

  node.add_child(std::move(x));
  node.close();
  this->send_stanza(node);
}

void XmppComponent::send_topic(const std::string& from, const std::string& topic, const std::string& to)
{
  XmlNode message("message");
  message["to"] = to;
  message["from"] = from + "@" + this->served_hostname;
  message["type"] = "groupchat";
  XmlNode subject("subject");
  subject.set_inner(topic);
  subject.close();
  message.add_child(std::move(subject));
  message.close();
  this->send_stanza(message);
}

void XmppComponent::send_muc_message(const std::string& muc_name, const std::string& nick, const std::string body_str, const std::string& jid_to)
{
  Stanza message("message");
  message["to"] = jid_to;
  message["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  message["type"] = "groupchat";
  XmlNode body("body");
  body.set_inner(body_str);
  body.close();
  message.add_child(std::move(body));
  message.close();
  this->send_stanza(message);
}

void XmppComponent::send_muc_leave(std::string&& muc_name, std::string&& nick, std::string&& message, const std::string& jid_to, const bool self)
{
  Stanza presence("presence");
  presence["to"] = jid_to;
  presence["from"] = muc_name + "@" + this->served_hostname + "/" + nick;
  presence["type"] = "unavailable";
  if (!message.empty() || self)
    {
      XmlNode status("status");
      if (!message.empty())
        status.set_inner(std::move(message));
      if (self)
        status["code"] = "110";
      status.close();
      presence.add_child(std::move(status));
    }
  presence.close();
  this->send_stanza(presence);
}
