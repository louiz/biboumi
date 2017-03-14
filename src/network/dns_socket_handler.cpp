#include <biboumi.h>
#ifdef UDNS_FOUND

#include <network/dns_socket_handler.hpp>
#include <network/dns_handler.hpp>
#include <network/poller.hpp>

#include <udns.h>

DNSSocketHandler::DNSSocketHandler(std::shared_ptr<Poller>& poller,
                                   const socket_t socket):
  SocketHandler(poller, socket)
{
  poller->add_socket_handler(this);
}

DNSSocketHandler::~DNSSocketHandler()
{
  this->unwatch();
}

void DNSSocketHandler::on_recv()
{
  dns_ioevent(nullptr, 0);
}

bool DNSSocketHandler::is_connected() const
{
  return true;
}

void DNSSocketHandler::unwatch()
{
  if (this->poller->is_managing_socket(this->socket))
    this->poller->remove_socket_handler(this->socket);
}

void DNSSocketHandler::watch()
{
  this->poller->add_socket_handler(this);
}

#endif /* UDNS_FOUND */
