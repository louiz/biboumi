#ifndef IRC_CLIENT_INCLUDED
# define IRC_CLIENT_INCLUDED

#include <libirc/irc_message.hpp>

#include <network/socket_handler.hpp>

#include <string>

/**
 * Represent one IRC client, i.e. an endpoint connected to a single IRC
 * server, through a TCP socket, receiving and sending commands to it.
 *
 * TODO: TLS support, maybe, but that's not high priority
 */
class IrcClient: public SocketHandler
{
public:
  explicit IrcClient();
  ~IrcClient();
  /**
   * We read the data, try to parse it and generate some event if
   * one or more full message is available.
   */
  void on_recv();
  /**
   * Just write as much data as possible on the socket.
   */
  void on_send();
  socket_t get_socket() const;
  /**
   * Connect to the remote server
   */
  void connect(const std::string& address, const std::string& port);
  /**
   * Called when successfully connected to the server
   */
  void on_connected();
  /**
   * Close the connection, remove us from the poller
   */
  void close();
  /**
   * Called when we detect an orderly close by the remote endpoint.
   */
  void on_connection_close();
  /**
   * Parse the data we have received so far and try to get one or more
   * complete messages from it.
   */
  void parse_in_buffer();
  /**
   * Serialize the given message into a line, and send that into the socket
   * (actually, into our out_buf and signal the poller that we want to wach
   * for send events to be ready)
   */
  void send_message(IrcMessage&& message);
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

private:
  socket_t socket;
  /**
   * Where data read from the socket is added, until we can parse a whole
   * IRC message, the used data is then removed from that buffer.
   *
   * TODO: something more efficient than a string.
   */
  std::string in_buf;
  /**
   * Where data is added, when we want to send something to the client.
   */
  std::string out_buf;

  IrcClient(const IrcClient&) = delete;
  IrcClient(IrcClient&&) = delete;
  IrcClient& operator=(const IrcClient&) = delete;
  IrcClient& operator=(IrcClient&&) = delete;
};

#endif // IRC_CLIENT_INCLUDED
