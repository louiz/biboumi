#include <libirc/irc_client.hpp>
#include <libirc/irc_message.hpp>
#include <network/poller.hpp>
#include <utils/scopeguard.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <cstring>
#include <netdb.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

IrcClient::IrcClient()
{
  std::cout << "IrcClient()" << std::endl;
  if ((this->socket = ::socket(AF_INET, SOCK_STREAM, 0)) == -1)
    throw std::runtime_error("Could not create socket");
}

IrcClient::~IrcClient()
{
  std::cout << "~IrcClient()" << std::endl;
}

void IrcClient::on_recv()
{
  char buf[4096];

  ssize_t size = ::recv(this->socket, buf, 4096, 0);
  if (0 == size)
    this->on_connection_close();
  else if (-1 == static_cast<ssize_t>(size))
    throw std::runtime_error("Error reading from socket");
  else
    {
      this->in_buf += std::string(buf, size);
      this->parse_in_buffer();
    }
}

void IrcClient::on_send()
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

socket_t IrcClient::get_socket() const
{
  return this->socket;
}

void IrcClient::connect(const std::string& address, const std::string& port)
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

void IrcClient::on_connected()
{
}

void IrcClient::on_connection_close()
{
  std::cout << "Connection closed by remote server." << std::endl;
  this->close();
}

void IrcClient::close()
{
  this->poller->remove_socket_handler(this->get_socket());
  ::close(this->socket);
}

void IrcClient::parse_in_buffer()
{
  while (true)
    {
      auto pos = this->in_buf.find("\r\n");
      if (pos == std::string::npos)
        break ;
      IrcMessage message(this->in_buf.substr(0, pos));
      this->in_buf = this->in_buf.substr(pos + 2, std::string::npos);
      std::cout << message << std::endl;
    }
}

void IrcClient::send_message(IrcMessage&& message)
{
  std::string res;
  if (!message.prefix.empty())
    res += ":" + std::move(message.prefix) + " ";
  res += std::move(message.command);
  for (const std::string& arg: message.arguments)
    {
      if (arg.find(" ") != std::string::npos)
        {
          res += " :" + arg;
          break;
        }
      res += " " + arg;
    }
  res += "\r\n";
  this->out_buf += res;
  if (!this->out_buf.empty())
    {
      this->poller->watch_send_events(this);
    }
}

void IrcClient::send_user_command(const std::string& username, const std::string& realname)
{
  this->send_message(IrcMessage("USER", {username, "NONE", "NONE", realname}));
}

void IrcClient::send_nick_command(const std::string& nick)
{
  this->send_message(IrcMessage("NICK", {nick}));
}

void IrcClient::send_join_command(const std::string& chan_name)
{
  this->send_message(IrcMessage("JOIN", {chan_name}));
}
