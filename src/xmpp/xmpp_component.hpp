#ifndef XMPP_COMPONENT_INCLUDED
# define XMPP_COMPONENT_INCLUDED

#include <network/socket_handler.hpp>
#include <xmpp/xmpp_parser.hpp>
#include <bridge/bridge.hpp>

#include <unordered_map>
#include <memory>
#include <string>

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
   * Send a message from from@served_hostname, with the given body
   */
  void send_message(const std::string& from, const std::string& body, const std::string& to);
  /**
   * Send a join from a new participant
   */
  void send_user_join(const std::string& from, const std::string& nick, const std::string& to);
  /**
   * Send the self join to the user
   */
  void send_self_join(const std::string& from, const std::string& nick, const std::string& to);
  /**
   * Send the MUC topic to the user
   */
  void send_topic(const std::string& from, const std::string& topic, const std::string& to);
  /**
   * Handle the various stanza types
   */
  void handle_handshake(const Stanza& stanza);
  void handle_presence(const Stanza& stanza);

private:
  /**
   * Return the bridge associated with the given full JID. Create a new one
   * if none already exist.
   */
  Bridge* get_user_bridge(const std::string& user_jid);

  XmppParser parser;
  std::string stream_id;
  std::string served_hostname;
  std::string secret;
  bool authenticated;

  std::unordered_map<std::string, std::function<void(const Stanza&)>> stanza_handlers;

  /**
   * One bridge for each user of the component. Indexed by the user's full
   * jid
   */
  std::unordered_map<std::string, std::unique_ptr<Bridge>> bridges;

  XmppComponent(const XmppComponent&) = delete;
  XmppComponent(XmppComponent&&) = delete;
  XmppComponent& operator=(const XmppComponent&) = delete;
  XmppComponent& operator=(XmppComponent&&) = delete;
};

#endif // XMPP_COMPONENT_INCLUDED

