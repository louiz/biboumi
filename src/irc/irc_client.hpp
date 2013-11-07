#ifndef IRC_CLIENT_INCLUDED
# define IRC_CLIENT_INCLUDED

#include <irc/irc_message.hpp>
#include <irc/irc_channel.hpp>
#include <irc/iid.hpp>

#include <network/socket_handler.hpp>

#include <unordered_map>
#include <string>

class Bridge;

/**
 * Represent one IRC client, i.e. an endpoint connected to a single IRC
 * server, through a TCP socket, receiving and sending commands to it.
 *
 * TODO: TLS support, maybe, but that's not high priority
 */
class IrcClient: public SocketHandler
{
public:
  explicit IrcClient(const std::string& hostname, const std::string& username, Bridge* bridge);
  ~IrcClient();
  /**
   * Connect to the IRC server
   */
  void start();
  /**
   * Called when successfully connected to the server
   */
  void on_connected();
  /**
   * Close the connection, remove us from the poller
   */
  void on_connection_close();
  /**
   * Parse the data we have received so far and try to get one or more
   * complete messages from it.
   */
  void parse_in_buffer();
  /**
   * Return the channel with this name, create it if it does not yet exist
   */
  IrcChannel* get_channel(const std::string& name);
  /**
   * Return our own nick
   */
  std::string get_own_nick() const;
  /**
   * Serialize the given message into a line, and send that into the socket
   * (actually, into our out_buf and signal the poller that we want to wach
   * for send events to be ready)
   */
  void send_message(IrcMessage&& message);
  /**
   * Send the PONG irc command
   */
  void send_pong_command(const IrcMessage& message);
  /**
   * Send the USER irc command
   */
  void send_user_command(const std::string& username, const std::string& realname);
  /**
   * Send the NICK irc command
   */
  void send_nick_command(const std::string& username);
  /**
   * Send the JOIN irc command
   */
  void send_join_command(const std::string& chan_name);
  /**
   * Send a PRIVMSG command for a channel
   * Return true if the message was actually sent
   */
  bool send_channel_message(const std::string& chan_name, const std::string& body);
  /**
   * Forward the server message received from IRC to the XMPP component
   */
  void forward_server_message(const IrcMessage& message);
  /**
   * Forward the join of an other user into an IRC channel, and save the
   * IrcUsers in the IrcChannel
   */
  void set_and_forward_user_list(const IrcMessage& message);
  /**
   * Remember our nick and host, when we are joined to the channel. The list
   * of user comes after so we do not send the self-presence over XMPP yet.
   */
  void on_self_channel_join(const IrcMessage& message);
  /**
   * When a channel message is received
   */
  void on_channel_message(const IrcMessage& message);
  /**
   * Save the topic in the IrcChannel
   */
  void on_topic_received(const IrcMessage& message);
  /**
   * The channel has been completely joined (self presence, topic, all names
   * received etc), send the self presence and topic to the XMPP user.
   */
  void on_channel_completely_joined(const IrcMessage& message);
  /**
   * When a message 001 is received, join the rooms we wanted to join, and set our actual nickname
   */
  void on_welcome_message(const IrcMessage& message);

private:
  /**
   * The hostname of the server we are connected to.
   */
  const std::string hostname;
  /**
   * The user name used in the USER irc command
   */
  const std::string username;
  /**
   * Our current nickname on the server
   */
  std::string current_nick;
  /**
   * Raw pointer because the bridge owns us.
   */
  Bridge* bridge;
  /**
   * The list of joined channels, indexed by name
   */
  std::unordered_map<std::string, std::unique_ptr<IrcChannel>> channels;
  /**
   * A list of chan we want to join, but we need a response 001 from
   * the server before sending the actual JOIN commands. So we just keep the
   * channel names in a list, and send the JOIN commands for each of them
   * whenever the WELCOME message is received.
   */
  std::vector<std::string> channels_to_join;
  bool welcomed;

  IrcClient(const IrcClient&) = delete;
  IrcClient(IrcClient&&) = delete;
  IrcClient& operator=(const IrcClient&) = delete;
  IrcClient& operator=(IrcClient&&) = delete;
};

#endif // IRC_CLIENT_INCLUDED
