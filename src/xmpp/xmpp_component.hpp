#ifndef XMPP_COMPONENT_INCLUDED
# define XMPP_COMPONENT_INCLUDED

#include <string>

#include <network/socket_handler.hpp>

#include <xmpp/xmpp_parser.hpp>

#include <unordered_map>

/**
 * An XMPP component, communicating with an XMPP server using the protocole
 * described in XEP-0114: Jabber Component Protocol
 *
 * TODO: implement XEP-0225: Component Connections
 */
class XmppComponent: public SocketHandler
{
public:
  explicit XmppComponent(const std::string& hostname, const std::string& secret);
  ~XmppComponent();
  void on_connected();
  void on_connection_close();
  void parse_in_buffer();

  /**
   * Connect to the XMPP server
   */
  void start();
  /**
   * Serialize the stanza and add it to the out_buf to be sent to the
   * server.
   */
  void send_stanza(const Stanza& stanza);
  /**
   * Handle the opening of the remote stream
   */
  void on_remote_stream_open(const XmlNode& node);
  /**
   * Handle the closing of the remote stream
   */
  void on_remote_stream_close(const XmlNode& node);
  /**
   * Handle received stanzas
   */
  void on_stanza(const Stanza& stanza);
  /**
   * Send an error stanza. Message being the name of the element inside the
   * stanza, and explanation being a short human-readable sentence
   * describing the error.
   */
  void send_stream_error(const std::string& message, const std::string& explanation);
  /**
   * Send the closing signal for our document (not closing the connection though).
   */
  void close_document();

  /**
   * Handle the various stanza types
   */
  void handle_handshake(const Stanza& stanza);

private:
  XmppParser parser;
  std::string stream_id;
  std::string served_hostname;
  std::string secret;
  bool authenticated;

  std::unordered_map<std::string, std::function<void(const Stanza&)>> stanza_handlers;

  XmppComponent(const XmppComponent&) = delete;
  XmppComponent(XmppComponent&&) = delete;
  XmppComponent& operator=(const XmppComponent&) = delete;
  XmppComponent& operator=(XmppComponent&&) = delete;
};

#endif // XMPP_COMPONENT_INCLUDED

