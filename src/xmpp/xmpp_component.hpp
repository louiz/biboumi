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
  void on_connection_failed(const std::string& reason) override final;
  void on_connected() override final;
  void on_connection_close() override final;
  void parse_in_buffer(const size_t size) override final;
  /**
   * Send a "close" message to all our connected peers.  That message
   * depends on the protocol used (this may be a QUIT irc message, or a
   * <stream/>, etc).  We may also directly close the connection, or we may
   * wait for the remote peer to acknowledge it before closing.
   */
  void shutdown();
  bool is_document_open() const;
  /**
   * Run a check on all bridges, to remove all disconnected (socket is
   * closed, or no channel is joined) IrcClients. Some kind of garbage collector.
   */
  void clean();
  /**
   * Connect to the XMPP server.
   */
  void start();
  /**
   * Reset the component so we can use the component on a new XMPP stream
   */
  void reset();
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
   * Send error stanza, described in http://xmpp.org/rfcs/rfc6120.html#stanzas-error
   */
  void send_stanza_error(const std::string& kind, const std::string& to, const std::string& from,
                         const std::string& id, const std::string& error_type,
                         const std::string& defined_condition, const std::string& text);
  /**
   * Send the closing signal for our document (not closing the connection though).
   */
  void close_document();
  /**
   * Send a message from from@served_hostname, with the given body
   */
  void send_message(const std::string& from, Xmpp::body&& body, const std::string& to);
  /**
   * Send a join from a new participant
   */
  void send_user_join(const std::string& from,
                      const std::string& nick,
                      const std::string& realjid,
                      const std::string& affiliation,
                      const std::string& role,
                      const std::string& to,
                      const bool self);
  /**
   * Send an error to indicate that the user tried to join an invalid room
   */
  void send_invalid_room_error(const std::string& muc_jid,
                               const std::string& nick,
                               const std::string& to);
  /**
   * Send the MUC topic to the user
   */
  void send_topic(const std::string& from, Xmpp::body&& xmpp_topic, const std::string& to);
  /**
   * Send a (non-private) message to the MUC
   */
  void send_muc_message(const std::string& muc_name, const std::string& nick, Xmpp::body&& body, const std::string& jid_to);
  /**
   * Send an unavailable presence for this nick
   */
  void send_muc_leave(std::string&& muc_name, std::string&& nick, Xmpp::body&& message, const std::string& jid_to, const bool self);
  /**
   * Indicate that a participant changed his nick
   */
  void send_nick_change(const std::string& muc_name,
                        const std::string& old_nick,
                        const std::string& new_nick,
                        const std::string& affiliation,
                        const std::string& role,
                        const std::string& jid_to,
                        const bool self);
  /**
   * An user is kicked from a room
   */
  void kick_user(const std::string& muc_name,
                     const std::string& target,
                     const std::string& reason,
                     const std::string& author,
                     const std::string& jid_to);
  /**
   * Send a presence type=error with a conflict element
   */
  void send_nickname_conflict_error(const std::string& muc_name,
                                    const std::string& nickname,
                                    const std::string& jid_to);
  /**
   * Send a presence from the MUC indicating a change in the role and/or
   * affiliation of a participant
   */
  void send_affiliation_role_change(const std::string& muc_name,
                                    const std::string& target,
                                    const std::string& affiliation,
                                    const std::string& role,
                                    const std::string& jid_to);
  /**
   * Handle the various stanza types
   */
  void handle_handshake(const Stanza& stanza);
  void handle_presence(const Stanza& stanza);
  void handle_message(const Stanza& stanza);
  void handle_iq(const Stanza& stanza);
  void handle_error(const Stanza& stanza);

  /**
   * Whether or not we ever succeeded our authentication to the XMPP server
   */
  bool ever_auth;
  /**
   * Whether or not the last connection+auth attempt was successful
   */
  bool last_auth;

private:
  /**
   * Return the bridge associated with the given full JID. Create a new one
   * if none already exist.
   */
  Bridge* get_user_bridge(const std::string& user_jid);
  /**
   * Return a buffer provided by the XML parser, to read data directly into
   * it, and avoiding some unnecessary copy.
   */
  void* get_receive_buffer(const size_t size) const override final;
  XmppParser parser;
  std::string stream_id;
  std::string served_hostname;
  std::string secret;
  bool authenticated;
  /**
   * Whether or not OUR XMPP document is open
   */
  bool doc_open;

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

