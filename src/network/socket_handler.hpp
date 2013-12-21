#ifndef SOCKET_HANDLER_INCLUDED
# define SOCKET_HANDLER_INCLUDED

#include <string>

typedef int socket_t;

class Poller;

/**
 * An interface, with a series of callbacks that should be implemented in
 * subclasses that deal with a socket. These callbacks are called on various events
 * (read/write/timeout, etc) when they are notified to a poller
 * (select/poll/epoll etc)
 */
class SocketHandler
{
public:
  explicit SocketHandler();
  virtual ~SocketHandler() {}
  /**
   * Connect to the remote server, and call on_connected() if this succeeds
   */
  bool connect(const std::string& address, const std::string& port);
  /**
   * Set the pointer to the given Poller, to communicate with it.
   */
  void set_poller(Poller* poller);
  /**
   * Reads data in our in_buf and the call parse_in_buf, for the implementor
   * to handle the data received so far.
   */
  void on_recv(const size_t nb = 4096);
  /**
   * Write as much data from out_buf as possible, in the socket.
   */
  void on_send();
  /**
   * Add the given data to out_buf and tell our poller that we want to be
   * notified when a send event is ready.
   */
  void send_data(std::string&& data);
  /**
   * Returns the socket that should be handled by the poller.
   */
  socket_t get_socket() const;
  /**
   * Close the connection, remove us from the poller
   */
  void close();
  /**
   * Called when the connection is successful.
   */
  virtual void on_connected() = 0;
  /**
   * Called when we detect a disconnection from the remote host.
   */
  virtual void on_connection_close() = 0;
  /**
   * Handle/consume (some of) the data received so far. If some data is used, the in_buf
   * should be truncated, only the unused data should be left untouched.
   */
  virtual void parse_in_buffer() = 0;

protected:
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
  /**
   * A pointer to the poller that manages us, because we need to communicate
   * with it, sometimes (for example to tell it that he now needs to watch
   * write events for us). Do not ever try to delete it.
   *
   * And a raw pointer because we are not owning it, it is owning us
   * (actually it is sharing our ownership with a Bridge).
   */
  Poller* poller;

private:
  SocketHandler(const SocketHandler&) = delete;
  SocketHandler(SocketHandler&&) = delete;
  SocketHandler& operator=(const SocketHandler&) = delete;
  SocketHandler& operator=(SocketHandler&&) = delete;
};

#endif // SOCKET_HANDLER_INCLUDED
