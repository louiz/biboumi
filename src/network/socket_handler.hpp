#ifndef SOCKET_HANDLER_INTERFACE_HPP
# define SOCKET_HANDLER_INTERFACE_HPP

typedef int socket_t;

class SocketHandler
{
public:
  SocketHandler() {}
  virtual ~SocketHandler() {}
  virtual socket_t get_socket() const = 0;
  virtual void on_recv() = 0;
  virtual void on_send() = 0;
  virtual void connect() = 0;
  virtual bool is_connected() const = 0;
private:
  SocketHandler(const SocketHandler&) = delete;
  SocketHandler(SocketHandler&&) = delete;
  SocketHandler& operator=(const SocketHandler&) = delete;
  SocketHandler& operator=(SocketHandler&&) = delete;
};

#endif // SOCKET_HANDLER_INTERFACE_HPP
