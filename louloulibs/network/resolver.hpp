#ifndef RESOLVER_HPP_INCLUDED
#define RESOLVER_HPP_INCLUDED

#include "louloulibs.h"

#include <functional>
#include <memory>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

struct AddrinfoDeleter
{
  void operator()(struct addrinfo* addr)
  {
#ifdef CARES_FOUND
    while (addr)
      {
        delete addr->ai_addr;
        auto next = addr->ai_next;
        delete addr;
        addr = next;
      }
#else
    freeaddrinfo(addr);
#endif
  }
};

class Resolver
{
public:
  using ErrorCallbackType = std::function<void(const char*)>;
  using SuccessCallbackType = std::function<void(const struct addrinfo*)>;

  Resolver();
  ~Resolver() = default;

  bool is_resolving() const
  {
#ifdef CARES_FOUND
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
#ifdef CARES_FOUND
    this->resolved6 = false;
    this->resolved4 = false;
    this->resolving = false;
    this->cares_addrinfo = nullptr;
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
#ifdef CARES_FOUND
  void on_hostname4_resolved(int status, struct hostent* hostent);
  void on_hostname6_resolved(int status, struct hostent* hostent);

  void fill_ares_addrinfo4(const struct hostent* hostent);
  void fill_ares_addrinfo6(const struct hostent* hostent);

  void on_resolved();

  bool resolved4;
  bool resolved6;

  bool resolving;

  /**
   * When using c-ares to resolve the host asynchronously, we need the
   * c-ares callbacks to fill a structure (a struct addrinfo, for
   * compatibility with getaddrinfo and the rest of the code that works when
   * c-ares is not used) with all returned values (for example an IPv6 and
   * an IPv4). The pointer is given to the unique_ptr to manage its lifetime.
   */
  struct addrinfo* cares_addrinfo;
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

  Resolver(const Resolver&) = delete;
  Resolver(Resolver&&) = delete;
  Resolver& operator=(const Resolver&) = delete;
  Resolver& operator=(Resolver&&) = delete;
};

std::string addr_to_string(const struct addrinfo* rp);

#endif /* RESOLVER_HPP_INCLUDED */
