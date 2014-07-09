#include <config.h>
#ifdef CARES_FOUND

#include <network/dns_socket_handler.hpp>
#include <network/dns_handler.hpp>
#include <network/poller.hpp>

#include <ares.h>

DNSSocketHandler::DNSSocketHandler(std::shared_ptr<Poller> poller,
                                   const socket_t socket):
  SocketHandler(poller, socket)
{
}

DNSSocketHandler::~DNSSocketHandler()
{
}

void DNSSocketHandler::connect()
{
}

void DNSSocketHandler::on_recv()
{
  // always stop watching send and read events. We will re-watch them if the
  // next call to ares_fds tell us to
  this->poller->remove_socket_handler(this->socket);
  ::ares_process_fd(DNSHandler::instance.get_channel(), this->socket, ARES_SOCKET_BAD);
}

void DNSSocketHandler::on_send()
{
  // always stop watching send and read events. We will re-watch them if the
  // next call to ares_fds tell us to
  this->poller->remove_socket_handler(this->socket);
  ::ares_process_fd(DNSHandler::instance.get_channel(), ARES_SOCKET_BAD, this->socket);
}

bool DNSSocketHandler::is_connected() const
{
  return true;
}

#endif /* CARES_FOUND */
