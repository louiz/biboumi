#include <network/tcp_socket_handler.hpp>
#include <network/dns_handler.hpp>

#include <network/poller.hpp>

#include <logger/logger.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <unistd.h>
#include <cerrno>
#include <cstring>

#ifdef BOTAN_FOUND
# include <botan/hex.h>
# include <botan/tls_exceptn.h>

namespace
{
    Botan::AutoSeeded_RNG& get_rng()
    {
      static Botan::AutoSeeded_RNG rng{};
      return rng;
    }
    BiboumiTLSPolicy& get_policy()
    {
      static BiboumiTLSPolicy policy{};
      return policy;
    }
    Botan::TLS::Session_Manager_In_Memory& get_session_manager()
    {
      static Botan::TLS::Session_Manager_In_Memory session_manager{get_rng()};
      return session_manager;
    }
}
#endif

#ifndef UIO_FASTIOV
# define UIO_FASTIOV 8
#endif

using namespace std::string_literals;
using namespace std::chrono_literals;


TCPSocketHandler::TCPSocketHandler(std::shared_ptr<Poller>& poller):
  SocketHandler(poller, -1),
  use_tls(false)
#ifdef BOTAN_FOUND
  ,credential_manager(this)
#endif
{}

TCPSocketHandler::~TCPSocketHandler()
{
  if (this->poller->is_managing_socket(this->get_socket()))
    this->poller->remove_socket_handler(this->get_socket());
  if (this->socket != -1)
    {
      ::close(this->socket);
      this->socket = -1;
    }
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
      if (this->is_connecting())
        log_warning("Error connecting: ", strerror(errno));
      else
        log_warning("Error while reading from socket: ", strerror(errno));
      // Remember if we were connecting, or already connected when this
      // happened, because close() sets this->connecting to false
      const auto were_connecting = this->is_connecting();
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
  struct msghdr msg{};
  msg.msg_iov = msg_iov;
  msg.msg_iovlen = 0;
  for (const std::string& s: this->out_buf)
    {
      // unconsting the content of s is ok, sendmsg will never modify it
      msg_iov[msg.msg_iovlen].iov_base = const_cast<char*>(s.data());
      msg_iov[msg.msg_iovlen].iov_len = s.size();
      msg.msg_iovlen++;
      if (msg.msg_iovlen == UIO_FASTIOV)
        break;
    }
  ssize_t res = ::sendmsg(this->socket, &msg, MSG_NOSIGNAL);
  if (res < 0)
    {
      log_error("sendmsg failed: ", strerror(errno));
      this->on_connection_close(strerror(errno));
      this->close();
    }
  else
    {
      // remove all the strings that were successfully sent.
      auto it = this->out_buf.begin();
      while (it != this->out_buf.end())
        {
          if (static_cast<size_t>(res) >= it->size())
            {
              res -= it->size();
              ++it;
            }
          else
            {
              // If one string has partially been sent, we use substr to
              // crop it
              if (res > 0)
                *it = it->substr(res, std::string::npos);
              break;
            }
        }
      this->out_buf.erase(this->out_buf.begin(), it);
      if (this->out_buf.empty())
        this->poller->stop_watching_send_events(this);
    }
}

void TCPSocketHandler::close()
{
  if (this->is_connected() || this->is_connecting())
    this->poller->remove_socket_handler(this->get_socket());
  if (this->socket != -1)
    {
      ::close(this->socket);
      this->socket = -1;
    }
  this->in_buf.clear();
  this->out_buf.clear();
}

void TCPSocketHandler::send_data(std::string&& data)
{
#ifdef BOTAN_FOUND
  if (this->use_tls)
    try {
      this->tls_send(std::move(data));
    } catch (const Botan::TLS::TLS_Exception& e) {
      this->on_connection_close("TLS error: "s + e.what());
      this->close();
      return ;
    }
  else
#endif
    this->raw_send(std::move(data));
}

void TCPSocketHandler::raw_send(std::string&& data)
{
  if (data.empty())
    return ;
  this->out_buf.emplace_back(std::move(data));
  if (this->is_connected())
    this->poller->watch_send_events(this);
}

void TCPSocketHandler::send_pending_data()
{
  if (this->is_connected() && !this->out_buf.empty())
    this->poller->watch_send_events(this);
}

bool TCPSocketHandler::is_using_tls() const
{
  return this->use_tls;
}

void* TCPSocketHandler::get_receive_buffer(const size_t) const
{
  return nullptr;
}

void TCPSocketHandler::consume_in_buffer(const std::size_t size)
{
  this->in_buf = this->in_buf.substr(size, std::string::npos);
}

#ifdef BOTAN_FOUND
void TCPSocketHandler::start_tls(const std::string& address, const std::string& port)
{
  Botan::TLS::Server_Information server_info(address, "irc", std::stoul(port));
  this->tls = std::make_unique<Botan::TLS::Client>(
# if BOTAN_VERSION_CODE >= BOTAN_VERSION_CODE_FOR(1,11,32)
      *this,
# else
      [this](const Botan::byte* data, size_t size) { this->tls_emit_data(data, size); },
      [this](const Botan::byte* data, size_t size) { this->tls_record_received(0, data, size); },
      [this](Botan::TLS::Alert alert, const Botan::byte*, size_t) { this->tls_alert(alert); },
      [this](const Botan::TLS::Session& session) { return this->tls_session_established(session); },
# endif
      get_session_manager(), this->credential_manager, get_policy(),
      get_rng(), server_info, Botan::TLS::Protocol_Version::latest_tls_version());
}

void TCPSocketHandler::tls_recv()
{
  static constexpr size_t buf_size = 4096;
  Botan::byte recv_buf[buf_size];

  const ssize_t size = this->do_recv(recv_buf, buf_size);
  if (size > 0)
    {
      const bool was_active = this->tls->is_active();
      try {
        this->tls->received_data(recv_buf, static_cast<size_t>(size));
      } catch (const Botan::TLS::TLS_Exception& e) {
        // May happen if the server sends malformed TLS data (buggy server,
        // or more probably we are just connected to a server that sends
        // plain-text)
        this->on_connection_close("TLS error: "s + e.what());
        this->close();
        return ;
      }
      if (!was_active && this->tls->is_active())
        this->on_tls_activated();
    }
}

void TCPSocketHandler::tls_send(std::string&& data)
{
  // We may not be connected yet, or the tls session has
  // not yet been negociated
  if (this->tls && this->tls->is_active())
    {
      const bool was_active = this->tls->is_active();
      if (!this->pre_buf.empty())
        {
          this->tls->send(this->pre_buf.data(), this->pre_buf.size());
          this->pre_buf.clear();
        }
      if (!data.empty())
        this->tls->send(reinterpret_cast<const Botan::byte*>(data.data()),
                       data.size());
      if (!was_active && this->tls->is_active())
        this->on_tls_activated();
    }
  else
    this->pre_buf.insert(this->pre_buf.end(),
                         std::make_move_iterator(data.begin()),
                         std::make_move_iterator(data.end()));
}

void TCPSocketHandler::tls_record_received(uint64_t, const Botan::byte *data, size_t size)
{
  this->in_buf += std::string(reinterpret_cast<const char*>(data),
                              size);
  if (!this->in_buf.empty())
    this->parse_in_buffer(size);
}

void TCPSocketHandler::tls_emit_data(const Botan::byte *data, size_t size)
{
  this->raw_send(std::string(reinterpret_cast<const char*>(data), size));
}

void TCPSocketHandler::tls_alert(Botan::TLS::Alert alert)
{
  log_debug("tls_alert: ", alert.type_string());
}

bool TCPSocketHandler::tls_session_established(const Botan::TLS::Session& session)
{
  log_debug("Handshake with ", session.server_info().hostname(), " complete.",
            " Version: ", session.version().to_string(),
            " using ", session.ciphersuite().to_string());
  if (!session.session_id().empty())
    log_debug("Session ID ", Botan::hex_encode(session.session_id()));
  if (!session.session_ticket().empty())
    log_debug("Session ticket ", Botan::hex_encode(session.session_ticket()));
  return true;
}

#if BOTAN_VERSION_CODE >= BOTAN_VERSION_CODE_FOR(1,11,34)
void TCPSocketHandler::tls_verify_cert_chain(const std::vector<Botan::X509_Certificate>& cert_chain,
                                             const std::vector<std::shared_ptr<const Botan::OCSP::Response>>& ocsp_responses,
                                             const std::vector<Botan::Certificate_Store*>& trusted_roots,
                                             Botan::Usage_Type usage, const std::string& hostname,
                                             const Botan::TLS::Policy& policy)
{
  log_debug("Checking remote certificate for hostname ", hostname);
  try
    {
      Botan::TLS::Callbacks::tls_verify_cert_chain(cert_chain, ocsp_responses, trusted_roots, usage, hostname, policy);
      log_debug("Certificate is valid");
    }
  catch (const std::exception& tls_exception)
    {
      log_warning("TLS certificate check failed: ", tls_exception.what());
      std::exception_ptr exception_ptr{};
      if (this->abort_on_invalid_cert())
        exception_ptr = std::current_exception();

      check_tls_certificate(cert_chain, hostname, this->credential_manager.get_trusted_fingerprint(), exception_ptr);
    }
}
#endif

void TCPSocketHandler::on_tls_activated()
{
  this->send_data({});
}

#endif // BOTAN_FOUND
