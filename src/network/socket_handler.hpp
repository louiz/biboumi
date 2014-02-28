#ifndef SOCKET_HANDLER_INCLUDED
# define SOCKET_HANDLER_INCLUDED

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <utility>
#include <string>
#include <list>

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
   * (re-)Initialize the socket
   */
  void init_socket();
  /**
   * Connect to the remote server, and call on_connected() if this succeeds
   */
  void connect(const std::string& address, const std::string& port);
  void connect();
  /**
   * Set the pointer to the given Poller, to communicate with it.
   */
  void set_poller(Poller* poller);
  /**
   * Reads data in our in_buf and the call parse_in_buf, for the implementor
   * to handle the data received so far.
   */
  void on_recv();
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
   * Called when the connection fails. Not when it is closed later, just at
   * the connect() call.
   */
  virtual void on_connection_failed(const std::string& reason) = 0;
  /**
   * Called when we detect a disconnection from the remote host.
   */
  virtual void on_connection_close() = 0;
  /**
   * Handle/consume (some of) the data received so far.  The data to handle
   * may be in the in_buf buffer, or somewhere else, depending on what
   * get_receive_buffer() returned.  If some data is used from in_buf, it
   * should be truncated, only the unused data should be left untouched.
   *
   * The size argument is the size of the last chunk of data that was added to the buffer.
   */
  virtual void parse_in_buffer(const size_t size) = 0;
  bool is_connected() const;
  bool is_connecting() const;

protected:
  /**
   * Provide a buffer in which data can be directly received. This can be
   * used to avoid copying data into in_buf before using it. If no buffer
   * can provided, nullptr is returned (the default implementation does
   * that).
   */
  virtual void* get_receive_buffer(const size_t size) const;
  /**
   * The handled socket.
   */
  socket_t socket;
  /**
   * Where data read from the socket is added until we can extract a full
   * and meaningful “message” from it.
   *
   * TODO: something more efficient than a string.
   */
  std::string in_buf;
  /**
   * Where data is added, when we want to send something to the client.
   */
  std::list<std::string> out_buf;
  /**
   * A pointer to the poller that manages us, because we need to communicate
   * with it, sometimes (for example to tell it that he now needs to watch
   * write events for us). Do not ever try to delete it.
   *
   * And a raw pointer because we are not owning it, it is owning us
   * (actually it is sharing our ownership with a Bridge).
   */
  Poller* poller;
  /**
   * Hostname we are connected/connecting to
   */
  std::string address;
  /**
   * Port we are connected/connecting to
   */
  std::string port;
  /**
   * Keep the details of the addrinfo the triggered a EINPROGRESS error when
   * connect()ing to it, to reuse it directly when connect() is called
   * again.
   */
  struct sockaddr ai_addr;
  socklen_t ai_addrlen;

  bool connected;
  bool connecting;

private:
  SocketHandler(const SocketHandler&) = delete;
  SocketHandler(SocketHandler&&) = delete;
  SocketHandler& operator=(const SocketHandler&) = delete;
  SocketHandler& operator=(SocketHandler&&) = delete;
};

#endif // SOCKET_HANDLER_INCLUDED
