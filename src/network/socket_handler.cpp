#include <network/socket_handler.hpp>

#include <utils/scopeguard.hpp>
#include <network/poller.hpp>

#include <logger/logger.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>

#include <iostream>

#ifndef UIO_FASTIOV
# define UIO_FASTIOV 8
#endif

SocketHandler::SocketHandler():
  poller(nullptr),
  connected(false),
  connecting(false)
{
  this->init_socket();
}

void SocketHandler::init_socket()
{
  if ((this->socket = ::socket(AF_INET, SOCK_STREAM, 0)) == -1)
    throw std::runtime_error("Could not create socket");
  int optval = 1;
  if (::setsockopt(this->socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1)
    log_warning("Failed to enable TCP keepalive on socket: " << strerror(errno));
  // Set the socket on non-blocking mode.  This is useful to receive a EAGAIN
  // error when connect() would block, to not block the whole process if a
  // remote is not responsive.
  const int existing_flags = ::fcntl(this->socket, F_GETFL, 0);
  if ((existing_flags == -1) ||
      (::fcntl(this->socket, F_SETFL, existing_flags | O_NONBLOCK) == -1))
    throw std::runtime_error(std::string("Could not initialize socket: ") + strerror(errno));
}

void SocketHandler::connect(const std::string& address, const std::string& port)
{
  this->address = address;
  this->port = port;

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
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = 0;

      const int res = ::getaddrinfo(address.c_str(), port.c_str(), &hints, &addr_res);

      if (res != 0)
        {
          log_warning(std::string("getaddrinfo failed: ") + gai_strerror(res));
          this->close();
          this->on_connection_failed(gai_strerror(res));
          return ;
        }
      // Make sure the alloced structure is always freed at the end of the
      // function
      sg.add_callback([&addr_res](){ freeaddrinfo(addr_res); });
    }
  else
    {
      // This function is called again, use the saved addrinfo structure,
      // instead of re-doing the whole getaddrinfo process.  We insert only
      // the meaningful values in the structure, and indicate that these are
      // the only possible values with ai_next = NULL.
      addr_res = (struct addrinfo*)malloc(sizeof(struct addrinfo));
      if (!addr_res)
        {
          this->close();
          this->on_connection_failed("memory error");
          return ;
        }
      sg.add_callback([&addr_res](){ free(addr_res); });
      addr_res->ai_next = NULL;
      addr_res->ai_addr = &this->ai_addr;
      addr_res->ai_addrlen = this->ai_addrlen;
    }
  this->connecting = true;

  for (struct addrinfo* rp = addr_res; rp; rp = rp->ai_next)
    {
      if (::connect(this->socket, rp->ai_addr, rp->ai_addrlen) == 0
          || errno == EISCONN)
        {
          log_info("Connection success.");
          this->connected = true;
          this->connecting = false;
          this->on_connected();
          return ;
        }
      else if (errno == EINPROGRESS || errno == EALREADY)
        {   // retry this process later, when the socket
            // is ready to be written on.
          this->poller->watch_send_events(this);
          // Save the addrinfo structure, to use it on the next call
          this->ai_addrlen = rp->ai_addrlen;
          memcpy(&this->ai_addr, rp->ai_addr, this->ai_addrlen);
          return ;
        }
      log_info("Connection failed:" << strerror(errno));
    }
  log_error("All connection attempts failed.");
  this->close();
  this->on_connection_failed(strerror(errno));
  return ;
}

void SocketHandler::connect()
{
  this->connect(this->address, this->port);
}

void SocketHandler::set_poller(Poller* poller)
{
  this->poller = poller;
}

void SocketHandler::on_recv()
{
  static constexpr size_t buf_size = 4096;
  char buf[buf_size];
  void* recv_buf = this->get_receive_buffer(buf_size);

  if (recv_buf == nullptr)
    recv_buf = buf;

  ssize_t size = ::recv(this->socket, recv_buf, buf_size, 0);
  if (0 == size)
    {
      this->on_connection_close();
      this->close();
    }
  else if (-1 == size)
    {
      log_warning("Error while reading from socket: " << strerror(errno));
      this->on_connection_close();
      this->close();
    }
  else
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
  this->connected = false;
  this->connecting = false;
  this->in_buf.clear();
  this->out_buf.clear();
  this->port.clear();
  this->poller->remove_socket_handler(this->get_socket());
  ::close(this->socket);
  // recreate the socket for a potential future usage
  this->init_socket();
}

socket_t SocketHandler::get_socket() const
{
  return this->socket;
}

void SocketHandler::send_data(std::string&& data)
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
