#ifndef SOCKET_HANDLER_HPP
# define SOCKET_HANDLER_HPP

#include <config.h>
#include <memory>

class Poller;

typedef int socket_t;

class SocketHandler
{
public:
  explicit SocketHandler(std::shared_ptr<Poller> poller, const socket_t socket):
    poller(poller),
    socket(socket)
  {}
  virtual ~SocketHandler() {}
  virtual void on_recv() = 0;
  virtual void on_send() = 0;
  virtual void connect() = 0;
  virtual bool is_connected() const = 0;

  socket_t get_socket() const
  { return this->socket; }

protected:
  /**
   * A pointer to the poller that manages us, because we need to communicate
   * with it.
   */
  std::shared_ptr<Poller> poller;
  /**
   * The handled socket.
   */
  socket_t socket;

private:
  SocketHandler(const SocketHandler&) = delete;
  SocketHandler(SocketHandler&&) = delete;
  SocketHandler& operator=(const SocketHandler&) = delete;
  SocketHandler& operator=(SocketHandler&&) = delete;
};

#endif // SOCKET_HANDLER_HPP
