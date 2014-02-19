#include <network/socket_handler.hpp>

#include <utils/scopeguard.hpp>
#include <network/poller.hpp>

#include <logger/logger.hpp>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <netdb.h>
#include <stdio.h>

#include <iostream>

SocketHandler::SocketHandler():
  poller(nullptr),
  connected(false)
{
  if ((this->socket = ::socket(AF_INET, SOCK_STREAM, 0)) == -1)
    throw std::runtime_error("Could not create socket");
  int optval = 1;
  if (::setsockopt(this->socket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1)
    log_warning("Failed to enable TCP keepalive on socket: " << strerror(errno));
}

std::pair<bool, std::string> SocketHandler::connect(const std::string& address, const std::string& port)
{
  log_info("Trying to connect to " << address << ":" << port);
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = 0;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  struct addrinfo* addr_res;
  const int res = ::getaddrinfo(address.c_str(), port.c_str(), &hints, &addr_res);

  if (res != 0)
    {
      log_warning(std::string("getaddrinfo failed: ") + gai_strerror(res));
      this->close();
      return std::make_pair(false, gai_strerror(res));
    }

  // Make sure the alloced structure is always freed at the end of the
  // function
  utils::ScopeGuard sg([&addr_res](){ freeaddrinfo(addr_res); });

  for (struct addrinfo* rp = addr_res; rp; rp = rp->ai_next)
    {
      if (::connect(this->socket, rp->ai_addr, rp->ai_addrlen) == 0)
        {
          log_info("Connection success.");
          this->connected = true;
          this->on_connected();
          return std::make_pair(true, "");
        }
      log_info("Connection failed:" << strerror(errno));
    }
  log_error("All connection attempts failed.");
  this->close();
  return std::make_pair(false, "");
}

void SocketHandler::set_poller(Poller* poller)
{
  this->poller = poller;
}

void SocketHandler::on_recv(const size_t nb)
{
  char buf[4096];

  ssize_t size = ::recv(this->socket, buf, nb, 0);
  if (0 == size)
    {
      this->on_connection_close();
      this->close();
    }
  else if (-1 == static_cast<ssize_t>(size))
    {
      log_warning("Error while reading from socket: " << strerror(errno));
      this->on_connection_close();
      this->close();
    }
  else
    {
      this->in_buf += std::string(buf, size);
      this->parse_in_buffer();
    }
}

void SocketHandler::on_send()
{
  const ssize_t res = ::send(this->socket, this->out_buf.data(), this->out_buf.size(), 0);
  if (res == -1)
    {
      log_error("send failed: " << strerror(errno));
      this->on_connection_close();
      this->close();
    }
  else
    {
      this->out_buf = this->out_buf.substr(res, std::string::npos);
      if (this->out_buf.empty())
        this->poller->stop_watching_send_events(this);
    }
}

void SocketHandler::close()
{
  this->connected = false;
  this->poller->remove_socket_handler(this->get_socket());
  ::close(this->socket);
  // recreate the socket for a potential future usage
  if ((this->socket = ::socket(AF_INET, SOCK_STREAM, 0)) == -1)
    throw std::runtime_error("Could not create socket");
}

socket_t SocketHandler::get_socket() const
{
  return this->socket;
}

void SocketHandler::send_data(std::string&& data)
{
  this->out_buf += std::move(data);
  if (!this->out_buf.empty())
    {
      this->poller->watch_send_events(this);
    }
}

bool SocketHandler::is_connected() const
{
  return this->connected;
}
