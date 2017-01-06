#include <network/dns_handler.hpp>
#include <utils/timed_events.hpp>
#include <network/resolver.hpp>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <udns.h>

#include <fstream>
#include <cstdlib>
#include <sstream>
#include <chrono>
#include <map>

using namespace std::string_literals;

static std::map<int, std::string> dns_error_messages {
    {DNS_E_TEMPFAIL, "Timeout while contacting DNS servers"},
    {DNS_E_PROTOCOL, "Misformatted DNS reply"},
    {DNS_E_NXDOMAIN, "Domain name not found"},
    {DNS_E_NOMEM, "Out of memory"},
    {DNS_E_BADQUERY, "Misformatted domain name"}
};

Resolver::Resolver():
#ifdef UDNS_FOUND
  resolved4(false),
  resolved6(false),
  resolving(false),
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
#ifdef UDNS_FOUND
  this->port = port;
#endif

  this->start_resolving(hostname, port);
}

int Resolver::call_getaddrinfo(const char *name, const char* port, int flags)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = flags;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  struct addrinfo* addr_res = nullptr;
  const int res = ::getaddrinfo(name, port,
                                &hints, &addr_res);

  if (res == 0 && addr_res)
    {
      if (!this->addr)
        this->addr.reset(addr_res);
      else
        { // Append this result at the end of the linked list
          struct addrinfo *rp = this->addr.get();
          while (rp->ai_next)
            rp = rp->ai_next;
          rp->ai_next = addr_res;
        }
    }

  return res;
}

#ifdef UDNS_FOUND
void Resolver::start_resolving(const std::string& hostname, const std::string& port)
{
  this->resolving = true;
  this->resolved = false;
  this->resolved4 = false;
  this->resolved6 = false;

  this->error_msg.clear();
  this->addr.reset(nullptr);

  // We first try to use it as an IP address directly. We tell getaddrinfo
  // to NOT use any DNS resolution.
  if (this->call_getaddrinfo(hostname.data(), port.data(), AI_NUMERICHOST) == 0)
    {
      this->on_resolved();
      return;
    }

  // Then we look into /etc/hosts to translate the given hostname
  const auto hosts = this->look_in_etc_hosts(hostname);
  if (!hosts.empty())
    {
      for (const auto &host: hosts)
        this->call_getaddrinfo(host.data(), port.data(), AI_NUMERICHOST);
      this->on_resolved();
      return;
    }

  // And finally, we try a DNS resolution
  auto hostname6_resolved = [](dns_ctx*, dns_rr_a6* result, void* data)
  {
    Resolver* resolver = static_cast<Resolver*>(data);
    resolver->on_hostname6_resolved(result);
  };

  auto hostname4_resolved = [](dns_ctx*, dns_rr_a4* result, void* data)
  {
    Resolver* resolver = static_cast<Resolver*>(data);
    resolver->on_hostname4_resolved(result);
  };

  DNSHandler::watch();
  auto res = dns_submit_a4(nullptr, hostname.data(), 0, hostname4_resolved, this);
  if (!res)
    this->on_hostname4_resolved(nullptr);
  res = dns_submit_a6(nullptr, hostname.data(), 0, hostname6_resolved, this);
  if (!res)
    this->on_hostname6_resolved(nullptr);

  this->start_timer();
}

void Resolver::start_timer()
{
  const auto timeout = dns_timeouts(nullptr, -1, 0);
  if (timeout < 0)
    return;
  TimedEvent event(std::chrono::steady_clock::now() + std::chrono::seconds(timeout), [this]() { this->start_timer(); }, "DNS");
  TimedEventsManager::instance().add_event(std::move(event));
}

std::vector<std::string> Resolver::look_in_etc_hosts(const std::string &hostname)
{
  std::ifstream hosts("/etc/hosts");
  std::string line;

  std::vector<std::string> results;
  while (std::getline(hosts, line))
    {
      if (line.empty())
        continue;

      std::string ip;
      std::istringstream line_stream(line);
      line_stream >> ip;
      if (ip.empty() || ip[0] == '#')
        continue;

      std::string host;
      while (line_stream >> host && !host.empty() && host[0] != '#')
        {
          if (hostname == host)
            {
              results.push_back(ip);
              break;
            }
        }
    }
  return results;
}

void Resolver::on_hostname4_resolved(dns_rr_a4 *result)
{
  if (dns_active(nullptr) == 0)
    DNSHandler::unwatch();

  this->resolved4 = true;

  const auto status = dns_status(nullptr);

  if (status >= 0 && result)
    {
      char buf[INET6_ADDRSTRLEN];

      for (auto i = 0; i < result->dnsa4_nrr; ++i)
        {
          inet_ntop(AF_INET, &result->dnsa4_addr[i], buf, sizeof(buf));
          this->call_getaddrinfo(buf, this->port.data(), AI_NUMERICHOST);
        }
    }
  else
    {
      const auto error = dns_error_messages.find(status);
      if (error != end(dns_error_messages))
        this->error_msg = error->second;
    }

  if (this->resolved6 && this->resolved4)
    this->on_resolved();
}

void Resolver::on_hostname6_resolved(dns_rr_a6 *result)
{
  if (dns_active(nullptr) == 0)
    DNSHandler::unwatch();

  this->resolved6 = true;
  char buf[INET6_ADDRSTRLEN];

  const auto status = dns_status(nullptr);

  if (status >= 0 && result)
    {
      for (auto i = 0; i < result->dnsa6_nrr; ++i)
        {
          inet_ntop(AF_INET6, &result->dnsa6_addr[i], buf, sizeof(buf));
          this->call_getaddrinfo(buf, this->port.data(), AI_NUMERICHOST);
        }
    }

  if (this->resolved6 && this->resolved4)
    this->on_resolved();
}

void Resolver::on_resolved()
{
  this->resolved = true;
  this->resolving = false;
  if (!this->addr)
    {
      if (this->error_cb)
        this->error_cb(this->error_msg.data());
    }
  else
    {
      if (this->success_cb)
        this->success_cb(this->addr.get());
    }
}

#else  // ifdef UDNS_FOUND

void Resolver::start_resolving(const std::string& hostname, const std::string& port)
{
  // If the resolution fails, the addr will be unset
  this->addr.reset(nullptr);

  const auto res = this->call_getaddrinfo(hostname.data(), port.data(), 0);

  this->resolved = true;

  if (res != 0)
    {
      this->error_msg = gai_strerror(res);
      if (this->error_cb)
        this->error_cb(this->error_msg.data());
    }
  else
    {
      if (this->success_cb)
        this->success_cb(this->addr.get());
    }
}
#endif  // ifdef UDNS_FOUND

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
