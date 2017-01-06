#include <louloulibs.h>
#ifdef UDNS_FOUND

#include <network/dns_socket_handler.hpp>
#include <network/dns_handler.hpp>
#include <network/poller.hpp>

#include <utils/timed_events.hpp>

#include <udns.h>

#include <cstring>

class Resolver;

using namespace std::string_literals;

std::unique_ptr<DNSSocketHandler> DNSHandler::socket_handler{};

DNSHandler::DNSHandler(std::shared_ptr<Poller> poller)
{
  dns_init(nullptr, 0);
  const auto socket = dns_open(nullptr);
  if (socket == -1)
    throw std::runtime_error("Failed to initialize udns socket: "s + strerror(errno));

  DNSHandler::socket_handler = std::make_unique<DNSSocketHandler>(poller, socket);
}

void DNSHandler::destroy()
{
  DNSHandler::socket_handler.reset(nullptr);
  dns_close(nullptr);
}

void DNSHandler::watch()
{
  DNSHandler::socket_handler->watch();
}

void DNSHandler::unwatch()
{
  DNSHandler::socket_handler->unwatch();
}

#endif /* UDNS_FOUND */
