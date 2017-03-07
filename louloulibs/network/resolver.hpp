#pragma once

#include "louloulibs.h"

#include <functional>
#include <vector>
#include <memory>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#ifdef UDNS_FOUND
# include <udns.h>
#endif

class AddrinfoDeleter
{
 public:
  void operator()(struct addrinfo* addr)
  {
    freeaddrinfo(addr);
  }
};


class Resolver
{
public:

  using ErrorCallbackType = std::function<void(const char*)>;
  using SuccessCallbackType = std::function<void(const struct addrinfo*)>;

  Resolver();
  ~Resolver() = default;
  Resolver(const Resolver&) = delete;
  Resolver(Resolver&&) = delete;
  Resolver& operator=(const Resolver&) = delete;
  Resolver& operator=(Resolver&&) = delete;

  bool is_resolving() const
  {
#ifdef UDNS_FOUND
    return this->resolving;
#else
    return false;
#endif
  }

  bool is_resolved() const
  {
    return this->resolved;
  }

  const auto& get_result() const
  {
    return this->addr;
  }
  std::string get_error_message() const
  {
    return this->error_msg;
  }

  void clear()
  {
#ifdef UDNS_FOUND
    this->resolved6 = false;
    this->resolved4 = false;
    this->resolving = false;
    this->port.clear();
#endif
    this->resolved = false;
    this->addr.reset();
    this->error_msg.clear();
  }

  void resolve(const std::string& hostname, const std::string& port,
               SuccessCallbackType success_cb, ErrorCallbackType error_cb);

private:
  void start_resolving(const std::string& hostname, const std::string& port);
  std::vector<std::string> look_in_etc_hosts(const std::string& hostname);
  /**
   * Call getaddrinfo() on the given hostname or IP, and append the result
   * to our internal addrinfo list. Return getaddrinfo()’s return value.
   */
  int call_getaddrinfo(const char* name, const char* port, int flags);

#ifdef UDNS_FOUND
  void on_hostname4_resolved(dns_rr_a4 *result);
  void on_hostname6_resolved(dns_rr_a6 *result);
  /**
   * Called after one record (4 or 6) has been resolved.
   */
  void after_resolved();

  void start_timer();

  void on_resolved();

  bool resolved4;
  bool resolved6;

  bool resolving;

  std::string port;

#endif
 /**
  * Tells if we finished the resolution process. It doesn't indicate if it
  * was successful (it is true even if the result is an error).
  */
  bool resolved;
  std::string error_msg;

  std::unique_ptr<struct addrinfo, AddrinfoDeleter> addr;

  ErrorCallbackType error_cb;
  SuccessCallbackType success_cb;
};

std::string addr_to_string(const struct addrinfo* rp);
