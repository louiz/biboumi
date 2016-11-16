#pragma once

#include <louloulibs.h>
#ifdef CARES_FOUND

#include <network/socket_handler.hpp>
#include <ares.h>

/**
 * Manage a socket returned by ares_fds. We do not create, open or close the
 * socket ourself: this is done by c-ares.  We just call ares_process_fd()
 * with the correct parameters, depending on what can be done on that socket
 * (Poller reported it to be writable or readeable)
 */

class DNSHandler;

class DNSSocketHandler: public SocketHandler
{
public:
  explicit DNSSocketHandler(std::shared_ptr<Poller> poller, DNSHandler& handler, const socket_t socket);
  ~DNSSocketHandler() = default;
  DNSSocketHandler(const DNSSocketHandler&) = delete;
  DNSSocketHandler(DNSSocketHandler&&) = delete;
  DNSSocketHandler& operator=(const DNSSocketHandler&) = delete;
  DNSSocketHandler& operator=(DNSSocketHandler&&) = delete;

  /**
   * Just call dns_process_fd, c-ares will do its work of send()ing or
   * recv()ing the data it wants on that socket.
   */
  void on_recv() override final;
  void on_send() override final;
  /**
   * Always true, see the comment for connect()
   */
  bool is_connected() const override final;
  void remove_from_poller();

private:
  DNSHandler& handler;
};

#endif // CARES_FOUND
