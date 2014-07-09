#ifndef DNS_HANDLER_HPP_INCLUDED
#define DNS_HANDLER_HPP_INCLUDED

#include <config.h>
#ifdef CARES_FOUND

class TCPSocketHandler;
class Poller;
class DNSSocketHandler;

# include <ares.h>
# include <memory>
# include <string>
# include <list>

void on_hostname4_resolved(void* arg, int status, int, struct hostent* hostent);
void on_hostname6_resolved(void* arg, int status, int, struct hostent* hostent);

/**
 * Class managing DNS resolution.  It should only be statically instanciated
 * once in SocketHandler.  It manages ares channel and calls various
 * functions of that library.
 */

class DNSHandler
{
public:
  DNSHandler();
  ~DNSHandler() = default;
  void gethostbyname(const std::string& name, TCPSocketHandler* socket_handler,
                     int family);
  /**
   * Call ares_fds to know what fd needs to be watched by the poller, create
   * or destroy DNSSocketHandlers depending on the result.
   */
  void watch_dns_sockets(std::shared_ptr<Poller>& poller);
  /**
   * Destroy and stop watching all the DNS sockets. Then de-init the channel
   * and library.
   */
  void destroy();
  ares_channel& get_channel();

  static DNSHandler instance;

private:
  /**
   * The list of sockets that needs to be watched, according to the last
   * call to ares_fds.  DNSSocketHandlers are added to it or removed from it
   * in the watch_dns_sockets() method
   */
  std::list<std::unique_ptr<DNSSocketHandler>> socket_handlers;
  ares_channel channel;

  DNSHandler(const DNSHandler&) = delete;
  DNSHandler(DNSHandler&&) = delete;
  DNSHandler& operator=(const DNSHandler&) = delete;
  DNSHandler& operator=(DNSHandler&&) = delete;
};

#endif /* CARES_FOUND */
#endif /* DNS_HANDLER_HPP_INCLUDED */
