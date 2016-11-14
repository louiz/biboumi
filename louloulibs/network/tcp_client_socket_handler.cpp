#include <network/tcp_client_socket_handler.hpp>
#include <utils/timed_events.hpp>
#include <utils/scopeguard.hpp>
#include <network/poller.hpp>

#include <logger/logger.hpp>

#include <cstring>
#include <unistd.h>
#include <fcntl.h>

using namespace std::string_literals;

TCPClientSocketHandler::TCPClientSocketHandler(std::shared_ptr<Poller> poller):
   TCPSocketHandler(poller),
   hostname_resolution_failed(false),
   connected(false),
   connecting(false)
{}

TCPClientSocketHandler::~TCPClientSocketHandler()
{
  this->close();
}

void TCPClientSocketHandler::init_socket(const struct addrinfo* rp)
{
  if (this->socket != -1)
    ::close(this->socket);
  if ((this->socket = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1)
    throw std::runtime_error("Could not create socket: "s + std::strerror(errno));
  // Bind the socket to a specific address, if specified
  if (!this->bind_addr.empty())
    {
      // Convert the address from string format to a sockaddr that can be
      // used in bind()
      struct addrinfo* result;
      int err = ::getaddrinfo(this->bind_addr.data(), nullptr, nullptr, &result);
      if (err != 0 || !result)
        log_error("Failed to bind socket to ", this->bind_addr, ": ",
                  gai_strerror(err));
      else
        {
          utils::ScopeGuard sg([result](){ freeaddrinfo(result); });
          struct addrinfo* rp;
          int bind_error = 0;
          for (rp = result; rp; rp = rp->ai_next)
            {
              if ((bind_error = ::bind(this->socket,
                         reinterpret_cast<const struct sockaddr*>(rp->ai_addr),
                         rp->ai_addrlen)) == 0)
                break;
            }
          if (!rp)
            log_error("Failed to bind socket to ", this->bind_addr, ": ",
                      strerror(errno));
          else
            log_info("Socket successfully bound to ", this->bind_addr);
        }
    }
  int optval = 1;
  if (::setsockopt(this->socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1)
    log_warning("Failed to enable TCP keepalive on socket: ", strerror(errno));
  // Set the socket on non-blocking mode.  This is useful to receive a EAGAIN
  // error when connect() would block, to not block the whole process if a
  // remote is not responsive.
  const int existing_flags = ::fcntl(this->socket, F_GETFL, 0);
  if ((existing_flags == -1) ||
      (::fcntl(this->socket, F_SETFL, existing_flags | O_NONBLOCK) == -1))
    throw std::runtime_error("Could not initialize socket: "s + std::strerror(errno));
}

void TCPClientSocketHandler::connect(const std::string& address, const std::string& port, const bool tls)
{
  this->address = address;
  this->port = port;
  this->use_tls = tls;

  struct addrinfo* addr_res;

  if (!this->connecting)
    {
      // Get the addrinfo from getaddrinfo (or ares_gethostbyname), only if
      // this is the first call of this function.
      if (!this->resolver.is_resolved())
        {
          log_info("Trying to connect to ", address, ":", port);
          // Start the asynchronous process of resolving the hostname. Once
          // the addresses have been found and `resolved` has been set to true
          // (but connecting will still be false), TCPClientSocketHandler::connect()
          // needs to be called, again.
          this->resolver.resolve(address, port,
                                 [this](const struct addrinfo*)
                                 {
                                   log_debug("Resolution success, calling connect() again");
                                   this->connect();
                                 },
                                 [this](const char*)
                                 {
                                   log_debug("Resolution failed, calling connect() again");
                                   this->connect();
                                 });
          return;
        }
      else
        {
          // The c-ares resolved the hostname and the available addresses
          // where saved in the cares_addrinfo linked list. Now, just use
          // this list to try to connect.
          addr_res = this->resolver.get_result().get();
          if (!addr_res)
            {
              this->hostname_resolution_failed = true;
              const auto msg = this->resolver.get_error_message();
              this->close();
              this->on_connection_failed(msg);
              return ;
            }
        }
    }
  else
    { // This function is called again, use the saved addrinfo structure,
      // instead of re-doing the whole getaddrinfo process.
      addr_res = &this->addrinfo;
    }

  for (struct addrinfo* rp = addr_res; rp; rp = rp->ai_next)
    {
      if (!this->connecting)
        {
          try {
            this->init_socket(rp);
          }
          catch (const std::runtime_error& error) {
            log_error("Failed to init socket: ", error.what());
            break;
          }
        }

      this->display_resolved_ip(rp);

      if (::connect(this->socket, rp->ai_addr, rp->ai_addrlen) == 0
          || errno == EISCONN)
        {
          log_info("Connection success.");
          TimedEventsManager::instance().cancel("connection_timeout"s +
                                                std::to_string(this->socket));
          this->poller->add_socket_handler(this);
          this->connected = true;
          this->connecting = false;
#ifdef BOTAN_FOUND
          if (this->use_tls)
            this->start_tls(this->address, this->port);
#endif
          this->connection_date = std::chrono::system_clock::now();

          // Get our local TCP port and store it
          this->local_port = static_cast<uint16_t>(-1);
          if (rp->ai_family == AF_INET6)
            {
              struct sockaddr_in6 a;
              socklen_t l = sizeof(a);
              if (::getsockname(this->socket, (struct sockaddr*)&a, &l) != -1)
                this->local_port = ntohs(a.sin6_port);
            }
          else if (rp->ai_family == AF_INET)
            {
              struct sockaddr_in a;
              socklen_t l = sizeof(a);
              if (::getsockname(this->socket, (struct sockaddr*)&a, &l) != -1)
                this->local_port = ntohs(a.sin_port);
            }

          log_debug("Local port: ", this->local_port, ", and remote port: ", this->port);

          this->on_connected();
          return ;
        }
      else if (errno == EINPROGRESS || errno == EALREADY)
        {   // retry this process later, when the socket
            // is ready to be written on.
          this->connecting = true;
          this->poller->add_socket_handler(this);
          this->poller->watch_send_events(this);
          // Save the addrinfo structure, to use it on the next call
          this->ai_addrlen = rp->ai_addrlen;
          memcpy(&this->ai_addr, rp->ai_addr, this->ai_addrlen);
          memcpy(&this->addrinfo, rp, sizeof(struct addrinfo));
          this->addrinfo.ai_addr = reinterpret_cast<struct sockaddr*>(&this->ai_addr);
          this->addrinfo.ai_next = nullptr;
          // If the connection has not succeeded or failed in 5s, we consider
          // it to have failed
          TimedEventsManager::instance().add_event(
                                                   TimedEvent(std::chrono::steady_clock::now() + 5s,
                                                              std::bind(&TCPClientSocketHandler::on_connection_timeout, this),
                                                              "connection_timeout"s + std::to_string(this->socket)));
          return ;
        }
      log_info("Connection failed:", std::strerror(errno));
    }
  log_error("All connection attempts failed.");
  this->close();
  this->on_connection_failed(std::strerror(errno));
  return ;
}

void TCPClientSocketHandler::on_connection_timeout()
{
  this->close();
  this->on_connection_failed("connection timed out");
}

void TCPClientSocketHandler::connect()
{
  this->connect(this->address, this->port, this->use_tls);
}

void TCPClientSocketHandler::close()
{
  TimedEventsManager::instance().cancel("connection_timeout"s +
                                        std::to_string(this->socket));

  TCPSocketHandler::close();

  this->connected = false;
  this->connecting = false;
  this->port.clear();
  this->resolver.clear();
}

void TCPClientSocketHandler::display_resolved_ip(struct addrinfo* rp) const
{
  if (rp->ai_family == AF_INET)
    log_debug("Trying IPv4 address ", addr_to_string(rp));
  else if (rp->ai_family == AF_INET6)
    log_debug("Trying IPv6 address ", addr_to_string(rp));
}

bool TCPClientSocketHandler::is_connected() const
{
  return this->connected;
}

bool TCPClientSocketHandler::is_connecting() const
{
  return this->connecting || this->resolver.is_resolving();
}

std::string TCPClientSocketHandler::get_port() const
{
  return this->port;
}

bool TCPClientSocketHandler::match_port_pairt(const uint16_t local, const uint16_t remote) const
{
  const uint16_t remote_port = static_cast<uint16_t>(std::stoi(this->port));
  return this->is_connected() && local == this->local_port && remote == remote_port;
}
