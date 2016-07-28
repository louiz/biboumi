#include <louloulibs.h>
#ifdef CARES_FOUND

#include <network/dns_socket_handler.hpp>
#include <network/dns_handler.hpp>
#include <network/poller.hpp>

#include <ares.h>

DNSSocketHandler::DNSSocketHandler(std::shared_ptr<Poller> poller,
                                   DNSHandler& handler,
                                   const socket_t socket):
  SocketHandler(poller, socket),
  handler(handler)
{
}

void DNSSocketHandler::connect()
{
}

void DNSSocketHandler::on_recv()
{
  // always stop watching send and read events. We will re-watch them if the
  // next call to ares_fds tell us to
  this->handler.remove_all_sockets_from_poller();
  ::ares_process_fd(DNSHandler::instance.get_channel(), this->socket, ARES_SOCKET_BAD);
}

void DNSSocketHandler::on_send()
{
  // always stop watching send and read events. We will re-watch them if the
  // next call to ares_fds tell us to
  this->handler.remove_all_sockets_from_poller();
  ::ares_process_fd(DNSHandler::instance.get_channel(), ARES_SOCKET_BAD, this->socket);
}

bool DNSSocketHandler::is_connected() const
{
  return true;
}

void DNSSocketHandler::remove_from_poller()
{
  this->poller->remove_socket_handler(this->socket);
}

#endif /* CARES_FOUND */
