#include <network/socket_handler.hpp>

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

Botan::AutoSeeded_RNG SocketHandler::rng;
Permissive_Credentials_Manager SocketHandler::credential_manager;
Botan::TLS::Policy SocketHandler::policy;
Botan::TLS::Session_Manager_In_Memory SocketHandler::session_manager(SocketHandler::rng);

#endif

#ifndef UIO_FASTIOV
# define UIO_FASTIOV 8
#endif

using namespace std::string_literals;
using namespace std::chrono_literals;

namespace ph = std::placeholders;

SocketHandler::SocketHandler(std::shared_ptr<Poller> poller):
  socket(-1),
  poller(poller),
  use_tls(false),
  connected(false),
  connecting(false)
{}

void SocketHandler::init_socket(const struct addrinfo* rp)
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

void SocketHandler::connect(const std::string& address, const std::string& port, const bool tls)
{
  this->address = address;
  this->port = port;
  this->use_tls = tls;

  utils::ScopeGuard sg;

  struct addrinfo* addr_res;

  if (!this->connecting)
    {
      log_info("Trying to connect to " << address << ":" << port);
      // Get the addrinfo from getaddrinfo, only if this is the first call
      // of this function.
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
                           std::bind(&SocketHandler::on_connection_timeout, this),
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

void SocketHandler::on_connection_timeout()
{
  this->close();
  this->on_connection_failed("connection timed out");
}

void SocketHandler::connect()
{
  this->connect(this->address, this->port, this->use_tls);
}

void SocketHandler::on_recv()
{
#ifdef BOTAN_FOUND
  if (this->use_tls)
    this->tls_recv();
  else
#endif
    this->plain_recv();
}

void SocketHandler::plain_recv()
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

ssize_t SocketHandler::do_recv(void* recv_buf, const size_t buf_size)
{
  ssize_t size = ::recv(this->socket, recv_buf, buf_size, 0);
  if (0 == size)
    {
      this->on_connection_close();
      this->close();
    }
  else if (-1 == size)
    {
      log_warning("Error while reading from socket: " << strerror(errno));
      if (this->connecting)
        {
          this->close();
          this->on_connection_failed(strerror(errno));
        }
      else
        {
          this->close();
          this->on_connection_close();
        }
    }
  return size;
}

void SocketHandler::on_send()
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
      this->on_connection_close();
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

void SocketHandler::close()
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

socket_t SocketHandler::get_socket() const
{
  return this->socket;
}

void SocketHandler::send_data(std::string&& data)
{
#ifdef BOTAN_FOUND
  if (this->use_tls)
    this->tls_send(std::move(data));
  else
#endif
    this->raw_send(std::move(data));
}

void SocketHandler::raw_send(std::string&& data)
{
  if (data.empty())
    return ;
  this->out_buf.emplace_back(std::move(data));
  if (this->connected)
    this->poller->watch_send_events(this);
}

void SocketHandler::send_pending_data()
{
  if (this->connected && !this->out_buf.empty())
    this->poller->watch_send_events(this);
}

bool SocketHandler::is_connected() const
{
  return this->connected;
}

bool SocketHandler::is_connecting() const
{
  return this->connecting;
}

void* SocketHandler::get_receive_buffer(const size_t) const
{
  return nullptr;
}

#ifdef BOTAN_FOUND
void SocketHandler::start_tls()
{
  Botan::TLS::Server_Information server_info(this->address, "irc", std::stoul(this->port));
  this->tls = std::make_unique<Botan::TLS::Client>(
      std::bind(&SocketHandler::tls_output_fn, this, ph::_1, ph::_2),
      std::bind(&SocketHandler::tls_data_cb, this, ph::_1, ph::_2),
      std::bind(&SocketHandler::tls_alert_cb, this, ph::_1, ph::_2, ph::_3),
      std::bind(&SocketHandler::tls_handshake_cb, this, ph::_1),
      session_manager, credential_manager, policy,
      rng, server_info, Botan::TLS::Protocol_Version::latest_tls_version());
}

void SocketHandler::tls_recv()
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

void SocketHandler::tls_send(std::string&& data)
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

void SocketHandler::tls_data_cb(const Botan::byte* data, size_t size)
{
  this->in_buf += std::string(reinterpret_cast<const char*>(data),
                              size);
  if (!this->in_buf.empty())
    this->parse_in_buffer(size);
}

void SocketHandler::tls_output_fn(const Botan::byte* data, size_t size)
{
  this->raw_send(std::string(reinterpret_cast<const char*>(data), size));
}

void SocketHandler::tls_alert_cb(Botan::TLS::Alert alert, const Botan::byte*, size_t)
{
  log_debug("tls_alert: " << alert.type_string());
}

bool SocketHandler::tls_handshake_cb(const Botan::TLS::Session& session)
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

void SocketHandler::on_tls_activated()
{
  this->send_data("");
}
#endif // BOTAN_FOUND
