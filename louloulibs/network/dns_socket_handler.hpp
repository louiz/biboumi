#pragma once

#include <louloulibs.h>
#ifdef UDNS_FOUND

#include <network/socket_handler.hpp>

/**
 * Manage the UDP socket provided by udns, we do not create, open or close the
 * socket ourself: this is done by udns.  We only watch it for readability
 */
class DNSSocketHandler: public SocketHandler
{
public:
  explicit DNSSocketHandler(std::shared_ptr<Poller> poller, const socket_t socket);
  ~DNSSocketHandler();
  DNSSocketHandler(const DNSSocketHandler&) = delete;
  DNSSocketHandler(DNSSocketHandler&&) = delete;
  DNSSocketHandler& operator=(const DNSSocketHandler&) = delete;
  DNSSocketHandler& operator=(DNSSocketHandler&&) = delete;

  void on_recv() override final;

  /**
   * Always true, see the comment for connect()
   */
  bool is_connected() const override final;

  void watch();
  void unwatch();
};

#endif // UDNS_FOUND
