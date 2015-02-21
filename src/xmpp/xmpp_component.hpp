#ifndef XMPP_COMPONENT_INCLUDED
# define XMPP_COMPONENT_INCLUDED

#include <xmpp/adhoc_commands_handler.hpp>
#include <network/tcp_socket_handler.hpp>
#include <xmpp/xmpp_parser.hpp>
#include <bridge/bridge.hpp>

#include <unordered_map>
#include <memory>
#include <string>
#include <map>

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
#define VERSION_NS       "jabber:iq:version"
#define ADHOC_NS         "http://jabber.org/protocol/commands"
#define PING_NS          "urn:xmpp:ping"

/**
 * A callback called when the waited iq result is received (it is matched
 * against the iq id)
 */
using iq_responder_callback_t = std::function<void(Bridge* bridge, const Stanza& stanza)>;

/**
 * An XMPP component, communicating with an XMPP server using the protocole
 * described in XEP-0114: Jabber Component Protocol
 *
 * TODO: implement XEP-0225: Component Connections
 */
class XmppComponent: public TCPSocketHandler
{
public:
  explicit XmppComponent(std::shared_ptr<Poller> poller, const std::string& hostname, const std::string& secret);
  ~XmppComponent();

  void on_connection_failed(const std::string& reason) override final;
  void on_connected() override final;
  void on_connection_close(const std::string& error) override final;
  void parse_in_buffer(const size_t size) override final;

  /**
   * Returns the bridge for the given user. If it does not exist, return
   * nullptr.
   */
  Bridge* find_user_bridge(const std::string& user_jid);
  /**
   * Return a list of all the managed bridges.
   */
  std::list<Bridge*> get_bridges() const;

  /**
   * Returns a unique id, to be used in the 'id' element of our iq stanzas.
   */
  static std::string next_id();
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
                         const std::string& defined_condition, const std::string& text,
                         const bool fulljid=true);
  /**
   * Send the closing signal for our document (not closing the connection though).
   */
  void close_document();
  /**
   * Send a message from from@served_hostname, with the given body
   *
   * If fulljid is false, the provided 'from' doesn't contain the
   * server-part of the JID and must be added.
   */
  void send_message(const std::string& from, Xmpp::body&& body,
                    const std::string& to, const std::string& type,
                    const bool fulljid=false);
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
   * Send an error to indicate that the user tried to send a message to an
   * invalid user.
   */
  void send_invalid_user_error(const std::string& user_name,
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
  void send_muc_leave(const std::string& muc_name, std::string&& nick, Xmpp::body&& message, const std::string& jid_to, const bool self);
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
   * Send a generic presence error
   */
  void send_presence_error(const std::string& muc_name,
                           const std::string& nickname,
                           const std::string& jid_to,
                           const std::string& type,
                           const std::string& condition,
                           const std::string& error_code,
                           const std::string& text);
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
   * Send a result IQ with the gateway disco informations.
   */
  void send_self_disco_info(const std::string& id, const std::string& jid_to);
  /**
   * Send a result IQ with the given version, or the gateway version if the
   * passed string is empty.
   */
  void send_version(const std::string& id, const std::string& jid_to, const std::string& jid_from,
                    const std::string& version="");
  /**
   * Send the list of all available ad-hoc commands to that JID. The list is
   * different depending on what JID made the request.
   */
  void send_adhoc_commands_list(const std::string& id, const std::string& requester_jid);
  /**
   * Send an iq version request
   */
  void send_iq_version_request(const std::string& from,
                               const std::string& jid_to);
  /**
   * Send a ping request
   */
  void send_ping_request(const std::string& from,
                         const std::string& jid_to,
                         const std::string& id);
  /**
   * Send an empty iq of type result
   */
  void send_iq_result(const std::string& id, const std::string& to_jid, const std::string& from);
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
   * Whether or not this is the first consecutive try on connecting to the
   * XMPP server.  We use this to delay the connection attempt for a few
   * seconds, if it is not the first try.
   */
  bool first_connection_try;

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
  AdhocCommandsHandler adhoc_commands_handler;

  /**
   * A map of id -> callback.  When we want to wait for an iq result, we add
   * the callback to this map, with the iq id as the key. When an iq result
   * is received, we look for a corresponding callback in this map. If
   * found, we call it and remove it.
   */
  std::map<std::string, iq_responder_callback_t> waiting_iq;

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
