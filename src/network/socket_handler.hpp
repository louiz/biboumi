#ifndef SOCKET_HANDLER_INCLUDED
# define SOCKET_HANDLER_INCLUDED

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
  explicit SocketHandler():
    poller(nullptr)
  {}
  /**
   * Set the pointer to the given Poller, to communicate with it.
   */
  void set_poller(Poller* poller)
  {
    this->poller = poller;
  };
  /**
   * Happens when the socket is ready to be received from.
   */
  virtual void on_recv() = 0;
  /**
   * Happens when the socket is ready to be written to.
   */
  virtual void on_send() = 0;
  /**
   * Returns the socket that should be handled by the poller.
   */
  virtual socket_t get_socket() const = 0;
  /**
   * Close the connection.
   */
  virtual void close() = 0;

protected:
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
