#include <network/tcp_socket_handler.hpp>
#include <network/dns_handler.hpp>

#include <utils/timed_events.hpp>
#include <utils/scopeguard.hpp>
#include <network/poller.hpp>

#include <logger/logger.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <cstring>
#include <fcntl.h>
#include <stdio.h>

#include <iostream>

#ifdef BOTAN_FOUND
# include <botan/hex.h>

Botan::AutoSeeded_RNG TCPSocketHandler::rng;
Permissive_Credentials_Manager TCPSocketHandler::credential_manager;
Botan::TLS::Policy TCPSocketHandler::policy;
Botan::TLS::Session_Manager_In_Memory TCPSocketHandler::session_manager(TCPSocketHandler::rng);

#endif

#ifndef UIO_FASTIOV
# define UIO_FASTIOV 8
#endif

using namespace std::string_literals;
using namespace std::chrono_literals;

namespace ph = std::placeholders;

TCPSocketHandler::TCPSocketHandler(std::shared_ptr<Poller> poller):
  SocketHandler(poller, -1),
  use_tls(false),
  connected(false),
  connecting(false)
#ifdef CARES_FOUND
  ,resolved(false),
  resolved4(false),
  resolved6(false),
  cares_addrinfo(nullptr),
  cares_error()
#endif
{}

TCPSocketHandler::~TCPSocketHandler()
{
#ifdef CARES_FOUND
  this->free_cares_addrinfo();
#endif
}

void TCPSocketHandler::init_socket(const struct addrinfo* rp)
{
  if ((this->socket = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1)
    throw std::runtime_error("Could not create socket: "s + strerror(errno));
  int optval = 1;
  if (::setsockopt(this->socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1)
    log_warning("Failed to enable TCP keepalive on socket: " << strerror(errno));
  // Set the socket on non-blocking mode.  This is useful to receive a EAGAIN
  // error when connect() would block, to not block the whole process if a
  // remote is not responsive.
  const int existing_flags = ::fcntl(this->socket, F_GETFL, 0);
  if ((existing_flags == -1) ||
      (::fcntl(this->socket, F_SETFL, existing_flags | O_NONBLOCK) == -1))
    throw std::runtime_error("Could not initialize socket: "s + strerror(errno));
}

void TCPSocketHandler::connect(const std::string& address, const std::string& port, const bool tls)
{
  this->address = address;
  this->port = port;
  this->use_tls = tls;

  utils::ScopeGuard sg;

  struct addrinfo* addr_res;

  if (!this->connecting)
    {
      // Get the addrinfo from getaddrinfo (or ares_gethostbyname), only if
      // this is the first call of this function.
#ifdef CARES_FOUND
      if (!this->resolved)
        {
          log_info("Trying to connect to " << address << ":" << port);
          // Start the asynchronous process of resolving the hostname. Once
          // the addresses have been found and `resolved` has been set to true
          // (but connecting will still be false), TCPSocketHandler::connect()
          // needs to be called, again.
          DNSHandler::instance.gethostbyname(address, this, AF_INET6);
          DNSHandler::instance.gethostbyname(address, this, AF_INET);
          return;
        }
      else
        {
          // The c-ares resolved the hostname and the available addresses
          // where saved in the cares_addrinfo linked list. Now, just use
          // this list to try to connect.
          addr_res = this->cares_addrinfo;
          if (!addr_res)
            {
              this->close();
              this->on_connection_failed(this->cares_error);
              return ;
            }
        }
#else
      log_info("Trying to connect to " << address << ":" << port);
      struct addrinfo hints;
      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_flags = 0;
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = 0;

      const int res = ::getaddrinfo(address.c_str(), port.c_str(), &hints, &addr_res);

      if (res != 0)
        {
          log_warning("getaddrinfo failed: "s + gai_strerror(res));
          this->close();
          this->on_connection_failed(gai_strerror(res));
          return ;
        }
      // Make sure the alloced structure is always freed at the end of the
      // function
      sg.add_callback([&addr_res](){ freeaddrinfo(addr_res); });
#endif
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
            log_error("Failed to init socket: " << error.what());
            break;
          }
        }
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
            this->start_tls();
#endif
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
                                                              std::bind(&TCPSocketHandler::on_connection_timeout, this),
                                                              "connection_timeout"s + std::to_string(this->socket)));
          return ;
        }
      log_info("Connection failed:" << strerror(errno));
    }
  log_error("All connection attempts failed.");
  this->close();
  this->on_connection_failed(strerror(errno));
  return ;
}

void TCPSocketHandler::on_connection_timeout()
{
  this->close();
  this->on_connection_failed("connection timed out");
}

void TCPSocketHandler::connect()
{
  this->connect(this->address, this->port, this->use_tls);
}

void TCPSocketHandler::on_recv()
{
#ifdef BOTAN_FOUND
  if (this->use_tls)
    this->tls_recv();
  else
#endif
    this->plain_recv();
}

void TCPSocketHandler::plain_recv()
{
  static constexpr size_t buf_size = 4096;
  char buf[buf_size];
  void* recv_buf = this->get_receive_buffer(buf_size);

  if (recv_buf == nullptr)
    recv_buf = buf;

  const ssize_t size = this->do_recv(recv_buf, buf_size);

  if (size > 0)
    {
      if (buf == recv_buf)
        {
          // data needs to be placed in the in_buf string, because no buffer
          // was provided to receive that data directly. The in_buf buffer
          // will be handled in parse_in_buffer()
          this->in_buf += std::string(buf, size);
        }
      this->parse_in_buffer(size);
    }
}

ssize_t TCPSocketHandler::do_recv(void* recv_buf, const size_t buf_size)
{
  ssize_t size = ::recv(this->socket, recv_buf, buf_size, 0);
  if (0 == size)
    {
      this->on_connection_close("");
      this->close();
    }
  else if (-1 == size)
    {
      log_warning("Error while reading from socket: " << strerror(errno));
      // Remember if we were connecting, or already connected when this
      // happened, because close() sets this->connecting to false
      const auto were_connecting = this->connecting;
      this->close();
      if (were_connecting)
        this->on_connection_failed(strerror(errno));
      else
        this->on_connection_close(strerror(errno));
    }
  return size;
}

void TCPSocketHandler::on_send()
{
  struct iovec msg_iov[UIO_FASTIOV] = {};
  struct msghdr msg{nullptr, 0,
      msg_iov,
      0, nullptr, 0, 0};
  for (std::string& s: this->out_buf)
    {
      // unconsting the content of s is ok, sendmsg will never modify it
      msg_iov[msg.msg_iovlen].iov_base = const_cast<char*>(s.data());
      msg_iov[msg.msg_iovlen].iov_len = s.size();
      if (++msg.msg_iovlen == UIO_FASTIOV)
        break;
    }
  ssize_t res = ::sendmsg(this->socket, &msg, MSG_NOSIGNAL);
  if (res < 0)
    {
      log_error("sendmsg failed: " << strerror(errno));
      this->on_connection_close(strerror(errno));
      this->close();
    }
  else
    {
      // remove all the strings that were successfully sent.
      for (auto it = this->out_buf.begin();
           it != this->out_buf.end();)
        {
          if (static_cast<size_t>(res) >= (*it).size())
            {
              res -= (*it).size();
              it = this->out_buf.erase(it);
            }
          else
            {
              // If one string has partially been sent, we use substr to
              // crop it
              if (res > 0)
                (*it) = (*it).substr(res, std::string::npos);
              break;
            }
        }
      if (this->out_buf.empty())
        this->poller->stop_watching_send_events(this);
    }
}

void TCPSocketHandler::close()
{
  TimedEventsManager::instance().cancel("connection_timeout"s +
                                        std::to_string(this->socket));
  if (this->connected || this->connecting)
    this->poller->remove_socket_handler(this->get_socket());
  if (this->socket != -1)
    {
      ::close(this->socket);
      this->socket = -1;
    }
  this->connected = false;
  this->connecting = false;
  this->in_buf.clear();
  this->out_buf.clear();
  this->port.clear();
}

void TCPSocketHandler::send_data(std::string&& data)
{
#ifdef BOTAN_FOUND
  if (this->use_tls)
    this->tls_send(std::move(data));
  else
#endif
    this->raw_send(std::move(data));
}

void TCPSocketHandler::raw_send(std::string&& data)
{
  if (data.empty())
    return ;
  this->out_buf.emplace_back(std::move(data));
  if (this->connected)
    this->poller->watch_send_events(this);
}

void TCPSocketHandler::send_pending_data()
{
  if (this->connected && !this->out_buf.empty())
    this->poller->watch_send_events(this);
}

bool TCPSocketHandler::is_connected() const
{
  return this->connected;
}

bool TCPSocketHandler::is_connecting() const
{
#ifdef CARES_FOUND
  return this->connecting || !this->resolved;
#else
  return this->connecting;
#endif
}

void* TCPSocketHandler::get_receive_buffer(const size_t) const
{
  return nullptr;
}

#ifdef BOTAN_FOUND
void TCPSocketHandler::start_tls()
{
  Botan::TLS::Server_Information server_info(this->address, "irc", std::stoul(this->port));
  this->tls = std::make_unique<Botan::TLS::Client>(
      std::bind(&TCPSocketHandler::tls_output_fn, this, ph::_1, ph::_2),
      std::bind(&TCPSocketHandler::tls_data_cb, this, ph::_1, ph::_2),
      std::bind(&TCPSocketHandler::tls_alert_cb, this, ph::_1, ph::_2, ph::_3),
      std::bind(&TCPSocketHandler::tls_handshake_cb, this, ph::_1),
      session_manager, credential_manager, policy,
      rng, server_info, Botan::TLS::Protocol_Version::latest_tls_version());
}

void TCPSocketHandler::tls_recv()
{
  static constexpr size_t buf_size = 4096;
  char recv_buf[buf_size];

  const ssize_t size = this->do_recv(recv_buf, buf_size);
  if (size > 0)
    {
      const bool was_active = this->tls->is_active();
      this->tls->received_data(reinterpret_cast<const Botan::byte*>(recv_buf),
                              static_cast<size_t>(size));
      if (!was_active && this->tls->is_active())
        this->on_tls_activated();
    }
}

void TCPSocketHandler::tls_send(std::string&& data)
{
  if (this->tls->is_active())
    {
      const bool was_active = this->tls->is_active();
      if (!this->pre_buf.empty())
        {
          this->tls->send(reinterpret_cast<const Botan::byte*>(this->pre_buf.data()),
                         this->pre_buf.size());
          this->pre_buf = "";
        }
      if (!data.empty())
        this->tls->send(reinterpret_cast<const Botan::byte*>(data.data()),
                       data.size());
      if (!was_active && this->tls->is_active())
        this->on_tls_activated();
    }
  else
    this->pre_buf += data;
}

void TCPSocketHandler::tls_data_cb(const Botan::byte* data, size_t size)
{
  this->in_buf += std::string(reinterpret_cast<const char*>(data),
                              size);
  if (!this->in_buf.empty())
    this->parse_in_buffer(size);
}

void TCPSocketHandler::tls_output_fn(const Botan::byte* data, size_t size)
{
  this->raw_send(std::string(reinterpret_cast<const char*>(data), size));
}

void TCPSocketHandler::tls_alert_cb(Botan::TLS::Alert alert, const Botan::byte*, size_t)
{
  log_debug("tls_alert: " << alert.type_string());
}

bool TCPSocketHandler::tls_handshake_cb(const Botan::TLS::Session& session)
{
  log_debug("Handshake with " << session.server_info().hostname() << " complete."
            << " Version: " << session.version().to_string()
            << " using " << session.ciphersuite().to_string());
  if (!session.session_id().empty())
    log_debug("Session ID " << Botan::hex_encode(session.session_id()));
  if (!session.session_ticket().empty())
    log_debug("Session ticket " << Botan::hex_encode(session.session_ticket()));
  return true;
}

void TCPSocketHandler::on_tls_activated()
{
  this->send_data("");
}

#endif // BOTAN_FOUND

#ifdef CARES_FOUND

void TCPSocketHandler::on_hostname4_resolved(int status, struct hostent* hostent)
{
  this->resolved4 = true;
  if (status == ARES_SUCCESS)
    this->fill_ares_addrinfo4(hostent);
  else
    this->cares_error = ::ares_strerror(status);

  if (this->resolved4 && this->resolved6)
    {
      this->resolved = true;
      this->connect();
    }
}

void TCPSocketHandler::on_hostname6_resolved(int status, struct hostent* hostent)
{
  this->resolved6 = true;
  if (status == ARES_SUCCESS)
    this->fill_ares_addrinfo6(hostent);
  else
    this->cares_error = ::ares_strerror(status);

  if (this->resolved4 && this->resolved6)
    {
      this->resolved = true;
      this->connect();
    }
}

void TCPSocketHandler::fill_ares_addrinfo4(const struct hostent* hostent)
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

void TCPSocketHandler::fill_ares_addrinfo6(const struct hostent* hostent)
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

void TCPSocketHandler::free_cares_addrinfo()
{
  while (this->cares_addrinfo)
    {
      delete this->cares_addrinfo->ai_addr;
      auto next = this->cares_addrinfo->ai_next;
      delete this->cares_addrinfo;
      this->cares_addrinfo = next;
    }
}

#endif  // CARES_FOUND
