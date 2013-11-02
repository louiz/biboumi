#include <irc/irc_client.hpp>
#include <irc/irc_message.hpp>

#include <iostream>
#include <stdexcept>

IrcClient::IrcClient()
{
  std::cout << "IrcClient()" << std::endl;
}

IrcClient::~IrcClient()
{
  std::cout << "~IrcClient()" << std::endl;
}

void IrcClient::on_connected()
{
}

void IrcClient::on_connection_close()
{
  std::cout << "Connection closed by remote server." << std::endl;
  this->close();
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
  this->send_data(std::move(res));
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
