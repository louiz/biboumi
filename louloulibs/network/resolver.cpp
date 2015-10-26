#include <network/dns_handler.hpp>
#include <network/resolver.hpp>
#include <string.h>
#include <arpa/inet.h>

using namespace std::string_literals;

Resolver::Resolver():
#ifdef CARES_FOUND
  resolved4(false),
  resolved6(false),
  resolving(false),
  cares_addrinfo(nullptr),
  port{},
#endif
  resolved(false),
  error_msg{}
{
}

void Resolver::resolve(const std::string& hostname, const std::string& port,
                       SuccessCallbackType success_cb, ErrorCallbackType error_cb)
{
  this->error_cb = error_cb;
  this->success_cb = success_cb;
  this->port = port;

  this->start_resolving(hostname, port);
}

#ifdef CARES_FOUND
void Resolver::start_resolving(const std::string& hostname, const std::string&)
{
  this->resolving = true;
  this->resolved = false;
  this->resolved4 = false;
  this->resolved6 = false;

  this->error_msg.clear();
  this->cares_addrinfo = nullptr;

  auto hostname4_resolved = [](void* arg, int status, int,
                                  struct hostent* hostent)
    {
      Resolver* resolver = static_cast<Resolver*>(arg);
      resolver->on_hostname4_resolved(status, hostent);
    };
  auto hostname6_resolved = [](void* arg, int status, int,
                                  struct hostent* hostent)
    {
      Resolver* resolver = static_cast<Resolver*>(arg);
      resolver->on_hostname6_resolved(status, hostent);
    };

  DNSHandler::instance.gethostbyname(hostname, hostname6_resolved,
                                     this, AF_INET6);
  DNSHandler::instance.gethostbyname(hostname, hostname4_resolved,
                                     this, AF_INET);
}

void Resolver::on_hostname4_resolved(int status, struct hostent* hostent)
{
  this->resolved4 = true;
  if (status == ARES_SUCCESS)
    this->fill_ares_addrinfo4(hostent);
  else
    this->error_msg = ::ares_strerror(status);

  if (this->resolved4 && this->resolved6)
    this->on_resolved();
}

void Resolver::on_hostname6_resolved(int status, struct hostent* hostent)
{
  this->resolved6 = true;
  if (status == ARES_SUCCESS)
    this->fill_ares_addrinfo6(hostent);
  else
    this->error_msg = ::ares_strerror(status);

  if (this->resolved4 && this->resolved6)
    this->on_resolved();
}

void Resolver::on_resolved()
{
  this->resolved = true;
  this->resolving = false;
  if (!this->cares_addrinfo)
    {
      if (this->error_cb)
        this->error_cb(this->error_msg.data());
    }
  else
    {
      this->addr.reset(this->cares_addrinfo);
      if (this->success_cb)
        this->success_cb(this->addr.get());
    }
}

void Resolver::fill_ares_addrinfo4(const struct hostent* hostent)
{
  struct addrinfo* prev = this->cares_addrinfo;
  struct in_addr** address = reinterpret_cast<struct in_addr**>(hostent->h_addr_list);

  while (*address)
    {
      // Create a new addrinfo list element, and fill it
      struct addrinfo* current = new struct addrinfo;
      current->ai_flags = 0;
      current->ai_family = hostent->h_addrtype;
      current->ai_socktype = SOCK_STREAM;
      current->ai_protocol = 0;
      current->ai_addrlen = sizeof(struct sockaddr_in);

      struct sockaddr_in* addr = new struct sockaddr_in;
      addr->sin_family = hostent->h_addrtype;
      addr->sin_port = htons(strtoul(this->port.data(), nullptr, 10));
      addr->sin_addr.s_addr = (*address)->s_addr;

      current->ai_addr = reinterpret_cast<struct sockaddr*>(addr);
      current->ai_next = nullptr;
      current->ai_canonname = nullptr;

      current->ai_next = prev;
      this->cares_addrinfo = current;
      prev = current;
      ++address;
    }
}

void Resolver::fill_ares_addrinfo6(const struct hostent* hostent)
{
  struct addrinfo* prev = this->cares_addrinfo;
  struct in6_addr** address = reinterpret_cast<struct in6_addr**>(hostent->h_addr_list);

  while (*address)
    {
      // Create a new addrinfo list element, and fill it
      struct addrinfo* current = new struct addrinfo;
      current->ai_flags = 0;
      current->ai_family = hostent->h_addrtype;
      current->ai_socktype = SOCK_STREAM;
      current->ai_protocol = 0;
      current->ai_addrlen = sizeof(struct sockaddr_in6);

      struct sockaddr_in6* addr = new struct sockaddr_in6;
      addr->sin6_family = hostent->h_addrtype;
      addr->sin6_port = htons(strtoul(this->port.data(), nullptr, 10));
      ::memcpy(addr->sin6_addr.s6_addr, (*address)->s6_addr, 16);
      addr->sin6_flowinfo = 0;
      addr->sin6_scope_id = 0;

      current->ai_addr = reinterpret_cast<struct sockaddr*>(addr);
      current->ai_next = nullptr;
      current->ai_canonname = nullptr;

      current->ai_next = prev;
      this->cares_addrinfo = current;
      prev = current;
      ++address;
    }
}

#else  // ifdef CARES_FOUND

void Resolver::start_resolving(const std::string& hostname, const std::string& port)
{
  // If the resolution fails, the addr will be unset
  this->addr.reset(nullptr);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = 0;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  struct addrinfo* addr_res = nullptr;
  const int res = ::getaddrinfo(hostname.data(), port.data(),
                                &hints, &addr_res);

  this->resolved = true;

  if (res != 0)
    {
      this->error_msg = gai_strerror(res);
      if (this->error_cb)
        this->error_cb(this->error_msg.data());
    }
  else
    {
      this->addr.reset(addr_res);
      if (this->success_cb)
        this->success_cb(this->addr.get());
    }
}
#endif  // ifdef CARES_FOUND

std::string addr_to_string(const struct addrinfo* rp)
{
  char buf[INET6_ADDRSTRLEN];
  if (rp->ai_family == AF_INET)
    return ::inet_ntop(rp->ai_family,
                       &reinterpret_cast<sockaddr_in*>(rp->ai_addr)->sin_addr,
                       buf, sizeof(buf));
  else if (rp->ai_family == AF_INET6)
    return ::inet_ntop(rp->ai_family,
                       &reinterpret_cast<sockaddr_in6*>(rp->ai_addr)->sin6_addr,
                       buf, sizeof(buf));
  return {};
}
