#pragma once

#include <louloulibs.h>
#ifdef UDNS_FOUND

class Poller;

#include <network/dns_socket_handler.hpp>

#include <string>
#include <vector>
#include <memory>

class DNSHandler
{
public:
  DNSHandler(std::shared_ptr<Poller> poller);
  ~DNSHandler() = default;

  DNSHandler(const DNSHandler&) = delete;
  DNSHandler(DNSHandler&&) = delete;
  DNSHandler& operator=(const DNSHandler&) = delete;
  DNSHandler& operator=(DNSHandler&&) = delete;

  void destroy();

  static void watch();
  static void unwatch();

private:
  /**
   * Manager for the socket returned by udns, that we need to watch with the poller
   */
  static std::unique_ptr<DNSSocketHandler> socket_handler;
};

#endif /* UDNS_FOUND */
