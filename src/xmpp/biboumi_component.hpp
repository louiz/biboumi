#pragma once

#include <database/database.hpp>
#include <xmpp/xmpp_component.hpp>
#include <xmpp/jid.hpp>

#include <bridge/bridge.hpp>

#include <memory>
#include <string>
#include <map>

namespace db
{
class MucLogLine;
}
struct ListElement;

/**
 * A callback called when the waited iq result is received (it is matched
 * against the iq id)
 */
using iq_responder_callback_t = std::function<void(Bridge* bridge, const Stanza& stanza)>;

/**
 * Interact with the Biboumi Bridge
 */
class BiboumiComponent: public XmppComponent
{
public:
  explicit BiboumiComponent(std::shared_ptr<Poller>& poller, const std::string& hostname, const std::string& secret);
  ~BiboumiComponent() = default;

  BiboumiComponent(const BiboumiComponent&) = delete;
  BiboumiComponent(BiboumiComponent&&) = delete;
  BiboumiComponent& operator=(const BiboumiComponent&) = delete;
  BiboumiComponent& operator=(BiboumiComponent&&) = delete;

  void after_handshake() override final;

  /**
   * Returns the bridge for the given user. If it does not exist, return
   * nullptr.
   */
  Bridge* find_user_bridge(const std::string& full_jid);
  /**
   * Return a list of all the managed bridges.
   */
  std::vector<Bridge*> get_bridges() const;

  /**
   * Send a "close" message to all our connected peers.  That message
   * depends on the protocol used (this may be a QUIT irc message, or a
   * <stream/>, etc).  We may also directly close the connection, or we may
   * wait for the remote peer to acknowledge it before closing.
   */
  void shutdown();
  /**
   * Run a check on all bridges, to remove all disconnected (socket is
   * closed, or no channel is joined) IrcClients. Some kind of garbage collector.
   */
  void clean();
  /**
   * Send a result IQ with the gateway disco informations.
   */
  void send_self_disco_info(const std::string& id, const std::string& jid_to);
  /**
   * Send a result IQ with the disco informations regarding IRC server JIDs.
   */
  void send_irc_server_disco_info(const std::string& id, const std::string& jid_to, const std::string& jid_from);
  /**
   * Sends the allowed namespaces in MUC message, according to
   * http://xmpp.org/extensions/xep-0045.html#impl-service-traffic
   */
   void send_irc_channel_muc_traffic_info(const std::string& id, const std::string& jid_to, const std::string& jid_from);
   void send_irc_channel_disco_info(const std::string& id, const std::string& jid_to, const std::string& jid_from);
  /**
   * Send a ping request
   */
  void send_ping_request(const std::string& from,
                         const std::string& jid_to,
                         const std::string& id);
  /**
   * Send the channels list in one big stanza
   */
  void send_iq_room_list_result(const std::string& id, const std::string& to_jid, const std::string& from,
                                const ChannelList& channel_list, std::vector<ListElement>::const_iterator begin,
                                std::vector<ListElement>::const_iterator end, const ResultSetInfo& rs_info);
  void send_invitation(const std::string& room_target, const std::string& jid_to, const std::string& author_nick);
  void accept_subscription(const std::string& from, const std::string& to);
  void ask_subscription(const std::string& from, const std::string& to);
  void send_presence_to_contact(const std::string& from, const std::string& to, const std::string& type, const std::string& id="");
  /**
   * Handle the various stanza types
   */
  void handle_presence(const Stanza& stanza);
  void handle_message(const Stanza& stanza);
  void handle_iq(const Stanza& stanza);

#ifdef USE_DATABASE
  bool handle_mam_request(const Stanza& stanza);
  void send_archived_message(const Database::MucLogLine& log_line, const std::string& from, const std::string& to,
                             const std::string& queryid);
  bool handle_room_configuration_form_request(const std::string& from, const Jid& to, const std::string& id);
  bool handle_room_configuration_form(const XmlNode& query, const std::string& from, const Jid& to, const std::string& id);
#endif

  /**
   * Return the bridge associated with the bare JID. Create a new one
   * if none already exist.
   */
  Bridge* get_user_bridge(const std::string& user_jid);

private:
  /**
   * A map of id -> callback.  When we want to wait for an iq result, we add
   * the callback to this map, with the iq id as the key. When an iq result
   * is received, we look for a corresponding callback in this map. If
   * found, we call it and remove it.
   */
  std::map<std::string, iq_responder_callback_t> waiting_iq;

  /**
   * One bridge for each user of the component. Indexed by the user's bare
   * jid
   */
  std::unordered_map<std::string, std::unique_ptr<Bridge>> bridges;

  AdhocCommandsHandler irc_server_adhoc_commands_handler;
  AdhocCommandsHandler irc_channel_adhoc_commands_handler;
};


