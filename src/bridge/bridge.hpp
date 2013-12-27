#ifndef BRIDGE_INCLUDED
# define BRIDGE_INCLUDED

#include <irc/irc_client.hpp>
#include <bridge/colors.hpp>
#include <irc/irc_user.hpp>
#include <irc/iid.hpp>

#include <unordered_map>
#include <string>
#include <memory>

class XmppComponent;
class Poller;

/**
 * One bridge is spawned for each XMPP user that uses the component.  The
 * bridge spawns IrcClients when needed (when the user wants to join a
 * channel on a new server) and does the translation between the two
 * protocols.
 */
class Bridge
{
public:
  explicit Bridge(const std::string& user_jid, XmppComponent* xmpp, Poller* poller);
  ~Bridge();
  /**
   * QUIT all connected IRC servers.
   */
  void shutdown();
  /**
   * Remove all inactive IrcClients
   */
  void clean();
  static Xmpp::body make_xmpp_body(const std::string& str);
  /***
   **
   ** From XMPP to IRC.
   **
   **/

  /**
   * Try to join an irc_channel, does nothing and return true if the channel
   * was already joined.
   */
  bool join_irc_channel(const Iid& iid, const std::string& username);
  void send_channel_message(const Iid& iid, const std::string& body);
  void send_private_message(const Iid& iid, const std::string& body);
  void leave_irc_channel(Iid&& iid, std::string&& status_message);
  void send_irc_nick_change(const Iid& iid, const std::string& new_nick);
  void send_irc_kick(const Iid& iid, const std::string& target, const std::string& reason);

  /***
   **
   ** From IRC to XMPP.
   **
   **/

  /**
   * Send a message corresponding to a server NOTICE, the from attribute
   * should be juste the server hostname@irc.component.
   */
  void send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg);
  /**
   * Send the presence of a new user in the MUC.
   */
  void send_user_join(const std::string& hostname,
                      const std::string& chan_name,
                      const IrcUser* user);
  /**
   * Send the self presence of an user when the MUC is fully joined.
   */
  void send_self_join(const std::string& hostname, const std::string& chan_name, const std::string nick);
  /**
   * Send the topic of the MUC to the user
   */
  void send_topic(const std::string& hostname, const std::string& chan_name, const std::string topic);
  /**
   * Send a MUC message from some participant
   */
  void send_message(const Iid& iid, const std::string& nick, const std::string& body, const bool muc);
  /**
   * Send an unavailable presence from this participant
   */
  void send_muc_leave(Iid&& iid, std::string&& nick, const std::string& message, const bool self);
  /**
   * Send presences to indicate that an user old_nick (ourself if self ==
   * true) changed his nick to new_nick
   */
  void send_nick_change(Iid&& iid, const std::string& old_nick, const std::string& new_nick, const bool self);
  void kick_muc_user(Iid&& iid, const std::string& target, const std::string& reason, const std::string& author);
  void send_nickname_conflict_error(const Iid& iid, const std::string& nickname);

  /**
   * Misc
   */
  std::string get_own_nick(const Iid& iid);

private:
  /**
   * Returns the client for the given hostname, create one (and use the
   * username in this case) if none is found, and connect that newly-created
   * client immediately.
   */
  IrcClient* get_irc_client(const std::string& hostname, const std::string& username);
  /**
   * This version does not create the IrcClient if it does not exist, and
   * returns nullptr in that case
   */
  IrcClient* get_irc_client(const std::string& hostname);
  /**
   * The JID of the user associated with this bridge. Messages from/to this
   * JID are only managed by this bridge.
   */
  std::string user_jid;
  /**
   * One IrcClient for each IRC server we need to be connected to.
   * The pointer is shared by the bridge and the poller.
   */
  std::unordered_map<std::string, std::shared_ptr<IrcClient>> irc_clients;
  /**
   * A raw pointer, because we do not own it, the XMPP component owns us,
   * but we still need to communicate with it, when sending messages from
   * IRC to XMPP.
   */
  XmppComponent* xmpp;
  /**
   * Poller, to give it the IrcClients that we spawn, to make it manage
   * their sockets.
   * We don't own it.
   */
  Poller* poller;

  Bridge(const Bridge&) = delete;
  Bridge(Bridge&& other) = delete;
  Bridge& operator=(const Bridge&) = delete;
  Bridge& operator=(Bridge&&) = delete;
};

#endif // BRIDGE_INCLUDED
