#pragma once

#include <bridge/result_set_management.hpp>
#include <bridge/list_element.hpp>
#include <bridge/history_limit.hpp>

#include <irc/irc_message.hpp>
#include <irc/irc_client.hpp>
#include <bridge/colors.hpp>
#include <irc/irc_user.hpp>
#include <irc/iid.hpp>

#include <unordered_map>
#include <functional>
#include <exception>
#include <string>
#include <memory>

#include <biboumi.h>

class BiboumiComponent;
class Poller;
struct ResultSetInfo;

/**
 * A callback called for each IrcMessage we receive. If the message triggers
 * a response, it must send ore or more iq and return true (in that case it
 * is removed from the list), otherwise it must do nothing and just return
 * false.
 */
using irc_responder_callback_t = std::function<bool(const std::string& irc_hostname, const IrcMessage& message)>;

/**
 * One bridge is spawned for each XMPP user that uses the component.  The
 * bridge spawns IrcClients when needed (when the user wants to join a
 * channel on a new server) and does the translation between the two
 * protocols.
 */
class Bridge
{
public:
  explicit Bridge(std::string  user_jid, BiboumiComponent& xmpp, std::shared_ptr<Poller>& poller);
  ~Bridge() = default;

  Bridge(const Bridge&) = delete;
  Bridge(Bridge&& other) = delete;
  Bridge& operator=(const Bridge&) = delete;
  Bridge& operator=(Bridge&&) = delete;
  /**
   * QUIT all connected IRC servers.
   */
  void shutdown(const std::string& exit_message);
  /**
   * PART the given resource from all the channels
   */
  void remove_resource(const std::string& resource, const std::string& part_message);
  /**
   * Remove all inactive IrcClients
   */
  void clean();
  /**
   * Return the jid of the XMPP user using this bridge
   */
  const std::string& get_jid() const;
  std::string get_bare_jid() const;

  static Xmpp::body make_xmpp_body(const std::string& str, const std::string& encoding = "ISO-8859-1");
  /***
   **
   ** From XMPP to IRC.
   **
   **/

  /**
   * Try to join an irc_channel.
   */
  bool join_irc_channel(const Iid& iid, std::string nickname,
                        const std::string& password,
                        const std::string& resource,
                        HistoryLimit history_limit);

  void send_channel_message(const Iid& iid, const std::string& body, std::string id, std::vector<XmlNode> nodes_to_reflect);
  void send_private_message(const Iid& iid, const std::string& body, const std::string& type="PRIVMSG");
  void send_raw_message(const std::string& hostname, const std::string& body);
  void leave_irc_channel(Iid&& iid, const std::string& status_message, const std::string& resource);
  void send_irc_nick_change(const Iid& iid, const std::string& new_nick, const std::string& requesting_resource);
  void send_irc_kick(const Iid& iid, const std::string& target, const std::string& reason,
                     const std::string& iq_id, const std::string& to_jid);
  void set_channel_topic(const Iid& iid, std::string subject);
  void send_xmpp_version_to_irc(const Iid& iid, const std::string& name, const std::string& version,
                                const std::string& os);
  void send_irc_ping_result(const Iid& iid, const std::string& id);
  void send_irc_version_request(const std::string& irc_hostname, const std::string& target,
                                const std::string& iq_id, const std::string& to_jid,
                                const std::string& from_jid);
  void send_irc_channel_list_request(const Iid& iid, const std::string& iq_id, const std::string& to_jid,
                                     ResultSetInfo rs_info);
  /**
   * Check if the channel list contains what is needed to answer the RSM request,
   * if it does, send the iq result. If the list is complete but does not contain
   * everything, send the result anyway (because there are no more available
   * channels that could complete the list).
   *
   * Returns true if we sent the answer.
   */
  bool send_matching_channel_list(const ChannelList& channel_list,
                                  const ResultSetInfo& rs_info, const std::string& id, const std::string& to_jid,
                                  const std::string& from);
  void forward_affiliation_role_change(const Iid& iid, const std::string& from, const std::string& nick,
                                       const std::string& affiliation, const std::string& role, const std::string& id);
  /**
   * Directly send a CTCP PING request to the IRC user
   */
  void send_irc_user_ping_request(const std::string& irc_hostname, const std::string& nick,
                                  const std::string& iq_id, const std::string& to_jid,
                                  const std::string& from_jid);
  /**
   * First check if the participant is in the room, before sending a direct
   * CTCP PING request to the IRC user
   */
  void send_irc_participant_ping_request(const Iid& iid, const std::string& nick,
                                         const std::string& iq_id, const std::string& to_jid,
                                         const std::string& from_jid);
  /**
   * Directly send back a result if it's a gateway ping or if we are
   * connected to the given IRC server, an error otherwise.
   */
  void on_gateway_ping(const std::string& irc_hostname, const std::string& iq_id, const std::string& to_jid,
                       const std::string& from_jid);

  void send_irc_invitation(const Iid& iid, const std::string& to);

  /***
   **
   ** From IRC to XMPP.
   **
   **/

  /**
   * Send a message corresponding to a server NOTICE, the from attribute
   * should be juste the server hostname.
   */
  void send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg);
  /**
   * Send the presence of a new user in the MUC.
   */
  void send_user_join(const std::string& hostname, const std::string& chan_name,
                      const IrcUser* user, const char user_mode,
                      const bool self, const std::string& resource);
  void send_user_join(const std::string& hostname, const std::string& chan_name,
                      const IrcUser* user, const char user_mode,
                      const bool self);

  /**
   * Send the topic of the MUC to the user
   */
  void send_topic(const std::string& hostname, const std::string& chan_name, const std::string& topic, const std::string& who);
  void send_topic(const std::string& hostname, const std::string& chan_name, const std::string& topic, const std::string& who, const std::string& resource);
  /**
   * Send the MUC history to the user
   */
  void send_room_history(const std::string& hostname, const std::string& chan_name, const HistoryLimit& history_limit);
  void send_room_history(const std::string& hostname, std::string chan_name, const std::string& resource, const HistoryLimit& history_limit);
  /**
   * Send a message from a MUC participant or a direct message
   */
  void send_message(const Iid& iid, const std::string& nick, const std::string& body, const bool muc, const bool log=true);
  /**
   * Send a presence of type error, from a room.
   */
  void send_presence_error(const Iid& iid, const std::string& nick, const std::string& type, const std::string& condition, const std::string& error_code, const std::string& text);
  /**
   * Send an unavailable presence from this participant
   */
  void send_muc_leave(const Iid& iid, const IrcUser& nick,
                      const std::string& message, const bool self,
                      const bool user_requested,
                      const std::string& resource,
                      const IrcClient* client);
  /**
   * Send presences to indicate that an user old_nick (ourself if self ==
   * true) changed his nick to new_nick.  The user_mode is needed because
   * the xmpp presence needs ton contain the role and affiliation of the
   * user.
   */
  void send_nick_change(Iid&& iid,
                        const std::string& old_nick,
                        const std::string& new_nick,
                        const char user_mode,
                        const bool self);
  void kick_muc_user(Iid&& iid, const std::string& target, const std::string& reason, const std::string& author,
                       const bool self);
  void send_nickname_conflict_error(const Iid& iid, const std::string& nickname);
  /**
   * Send a role/affiliation change, matching the change of mode for that user
   */
  void send_affiliation_role_change(const Iid& iid, const std::string& target, const char mode);
  /**
   * Send an iq version request coming from nick!hostname@
   */
  void send_iq_version_request(const std::string& nick, const std::string& hostname);
  /**
   * Send an iq ping request coming from nick!hostname@
   */
  void send_xmpp_ping_request(const std::string& nick, const std::string& hostname,
                              const std::string& id);
  void send_xmpp_invitation(const Iid& iid, const std::string& author);
  void on_irc_client_connected(const std::string& hostname);
  void on_irc_client_disconnected(const std::string& hostname);

  /**
   * Misc
   */
  std::string get_own_nick(const Iid& iid);
  /**
   * Get the number of server to which this bridge is connected or connecting.
   */
  size_t active_clients() const;
  /**
   * Add (or replace the existing) <nick, jid> into the preferred_user_from map
   */
  void set_preferred_from_jid(const std::string& nick, const std::string& full_jid);
  /**
   * Remove the preferred jid for the given IRC nick
   */
  void remove_preferred_from_jid(const std::string& nick);
  /**
   * Given a channel_name, remove all preferred from_jid that come
   * from this chan.
   */
  void remove_all_preferred_from_jid_of_room(const std::string& channel_name);
  /**
   * Add a callback to the waiting list of irc callbacks.
   */
  void add_waiting_irc(irc_responder_callback_t&& callback);
  /**
   * Iter over all the waiting_iq, call the iq_responder_filter_t for each,
   * whenever one of them returns true: call the corresponding
   * iq_responder_callback_t and remove the callback from the list.
   */
  void trigger_on_irc_message(const std::string& irc_hostname, const IrcMessage& message);
  std::unordered_map<std::string, std::unique_ptr<IrcClient>>& get_irc_clients();
  const std::unordered_map<std::string, std::unique_ptr<IrcClient>>& get_irc_clients() const;
  std::set<char> get_chantypes(const std::string& hostname) const;
#ifdef USE_DATABASE
  void set_record_history(const bool val);
#endif

private:
  /**
   * Returns the client for the given hostname, create one (and use the
   * username in this case) if none is found, and connect that newly-created
   * client immediately.
   */
  IrcClient* make_irc_client(const std::string& hostname, const std::string& nickname);
  /**
   * This version does not create the IrcClient if it does not exist, throws
   * a IRCServerNotConnected error in that case.
   */
  IrcClient* get_irc_client(const std::string& hostname);
public:
  /**
   * Idem, but returns nullptr if the server does not exist.
   */
  IrcClient* find_irc_client(const std::string& hostname) const;
private:
  /**
   * The bare JID of the user associated with this bridge. Messages from/to this
   * JID are only managed by this bridge.
   */
  const std::string user_jid;
  /**
   * One IrcClient for each IRC server we need to be connected to.
   * The pointer is shared by the bridge and the poller.
   */
  std::unordered_map<std::string, std::unique_ptr<IrcClient>> irc_clients;
  /**
   * To communicate back with the XMPP component
   */
  BiboumiComponent& xmpp;
  /**
   * Poller, to give it the IrcClients that we spawn, to make it manage
   * their sockets.
   */
  std::shared_ptr<Poller> poller;
  /**
   * A map of <nick, full_jid>. For example if this map contains <"toto",
   * "#somechan%server@biboumi/ToTo">, whenever a private message is
   * received from the user "toto", instead of forwarding it to XMPP with
   * from='toto!server@biboumi', we use instead
   * from='#somechan%server@biboumi/ToTo'
   */
  std::unordered_map<std::string, std::string> preferred_user_from;
  /**
   * A list of callbacks that are waiting for some IrcMessage to trigger a
   * response.  We add callbacks in this list whenever we received an IQ
   * request and we need a response from IRC to be able to provide the
   * response iq.
   */
  std::vector<irc_responder_callback_t> waiting_irc;
  /**
   * Resources to IRC channel/server mapping:
   */
  using Resource = std::string;
  using ChannelName = std::string;
  using IrcHostname = std::string;
  using ChannelKey = std::tuple<ChannelName, IrcHostname>;
public:
  std::map<ChannelKey, std::set<Resource>> resources_in_chan;
  std::map<IrcHostname, std::set<Resource>> resources_in_server;
private:
  /**
   * Manage which resource is in which channel
   */
  void add_resource_to_chan(const ChannelKey& channel, const std::string& resource);
  void remove_resource_from_chan(const ChannelKey& channel, const std::string& resource);
public:
  bool is_resource_in_chan(const ChannelKey& channel, const std::string& resource) const;
private:
  void remove_all_resources_from_chan(const ChannelKey& channel);
  std::size_t number_of_resources_in_chan(const ChannelKey& channel) const;

  void add_resource_to_server(const IrcHostname& irc_hostname, const std::string& resource);
  void remove_resource_from_server(const IrcHostname& irc_hostname, const std::string& resource);
  size_t number_of_channels_the_resource_is_in(const std::string& irc_hostname, const std::string& resource) const;

  /**
   * Generate all the stanzas to be sent to this resource, simulating a join on this channel.
   * This means sending the whole user list, the topic, etc
   * TODO: send message history
   */
  void generate_channel_join_for_resource(const Iid& iid, const std::string& resource);
  /**
   * A cache of the channels list (as returned by the server on a LIST
   * request), to be re-used on a subsequent XMPP list request that
   * uses result-set-management.
   */
  std::map<IrcHostname, ChannelList> channel_list_cache;

#ifdef USE_DATABASE
  bool record_history { true };
#endif
};

struct IRCNotConnected: public std::exception
{
  IRCNotConnected(const std::string& hostname):
    hostname(hostname) {}
  const std::string hostname;
};


