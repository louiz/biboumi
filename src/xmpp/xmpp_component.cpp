#include <xmpp/xmpp_component.hpp>

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
  try
    {
      const auto& handler = this->stanza_handlers.at(stanza.get_name());
      handler(stanza);
    }
  catch (const std::out_of_range& exception)
    {
      std::cout << "No handler for stanza of type " << stanza.get_name() << std::endl;
      return;
    }
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
  this->authenticated = true;
}
