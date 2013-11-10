#include <network/socket_handler.hpp>

#include <utils/scopeguard.hpp>
#include <network/poller.hpp>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <netdb.h>
#include <stdio.h>

#include <iostream>

SocketHandler::SocketHandler():
  poller(nullptr)
{
  if ((this->socket = ::socket(AF_INET, SOCK_STREAM, 0)) == -1)
    throw std::runtime_error("Could not create socket");
}

void SocketHandler::connect(const std::string& address, const std::string& port)
{
  std::cout << "Trying to connect to " << address << ":" << port << std::endl;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = 0;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;

  struct addrinfo* addr_res;
  const int res = ::getaddrinfo(address.c_str(), port.c_str(), &hints, &addr_res);
  // Make sure the alloced structure is always freed at the end of the
  // function
  utils::ScopeGuard sg([&addr_res](){ freeaddrinfo(addr_res); });

  if (res != 0)
    {
      perror("getaddrinfo");
      throw std::runtime_error("getaddrinfo failed");
    }
  for (struct addrinfo* rp = addr_res; rp; rp = rp->ai_next)
    {
      std::cout << "One result" << std::endl;
      if (::connect(this->socket, rp->ai_addr, rp->ai_addrlen) == 0)
        {
          std::cout << "Connection success." << std::endl;
          this->on_connected();
          return ;
        }
      std::cout << "Connection failed:" << std::endl;
      perror("connect");
    }
  std::cout << "All connection attempts failed." << std::endl;
  this->close();
}

void SocketHandler::set_poller(Poller* poller)
{
  this->poller = poller;
}

void SocketHandler::on_recv()
{
  char buf[4096];

  ssize_t size = ::recv(this->socket, buf, 4096, 0);
  if (0 == size)
    {
      this->on_connection_close();
      this->close();
    }
  else if (-1 == static_cast<ssize_t>(size))
    throw std::runtime_error("Error reading from socket");
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
      perror("send");
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
  this->poller->remove_socket_handler(this->get_socket());
  ::close(this->socket);
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
