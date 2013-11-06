#include <irc/irc_message.hpp>
#include <irc/irc_client.hpp>
#include <bridge/bridge.hpp>
#include <irc/irc_user.hpp>

#include <utils/make_unique.hpp>
#include <utils/split.hpp>

#include <iostream>
#include <stdexcept>

IrcClient::IrcClient(const std::string& hostname, const std::string& username, Bridge* bridge):
  hostname(hostname),
  username(username),
  bridge(bridge)
{
  std::cout << "IrcClient()" << std::endl;
}

IrcClient::~IrcClient()
{
  std::cout << "~IrcClient()" << std::endl;
}

void IrcClient::start()
{
  this->connect(this->hostname, "6667");
}

void IrcClient::on_connected()
{
  this->send_nick_command(this->username);
  this->send_user_command(this->username, this->username);
}

void IrcClient::on_connection_close()
{
  std::cout << "Connection closed by remote server." << std::endl;
}

IrcChannel* IrcClient::get_channel(const std::string& name)
{
  try
    {
      return this->channels.at(name).get();
    }
  catch (const std::out_of_range& exception)
    {
      this->channels.emplace(name, std::make_unique<IrcChannel>());
      return this->channels.at(name).get();
    }
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
      // TODO map function and command name properly
      if (message.command == "PING")
        this->send_pong_command();
      else if (message.command == "NOTICE" ||
               message.command == "375" ||
               message.command == "372")
        this->forward_server_message(message);
      else if (message.command == "JOIN")
        this->on_self_channel_join(message);
      else if (message.command == "353")
        this->set_and_forward_user_list(message);
      else if (message.command == "332")
        this->on_topic_received(message);
      else if (message.command == "366")
        this->on_channel_completely_joined(message);
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
  std::cout << "=== IRC SENDING ===" << std::endl;
  std::cout << res << std::endl;
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
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->joined == false)
    this->send_message(IrcMessage("JOIN", {chan_name}));
}

void IrcClient::send_pong_command()
{
  this->send_message(IrcMessage("PONG", {}));
}

void IrcClient::forward_server_message(const IrcMessage& message)
{
  const std::string from = message.prefix;
  const std::string body = message.arguments[1];

  this->bridge->send_xmpp_message(this->hostname, from, body);
}

void IrcClient::set_and_forward_user_list(const IrcMessage& message)
{
  const std::string chan_name = message.arguments[2];
  IrcChannel* channel = this->get_channel(chan_name);
  std::vector<std::string> nicks = utils::split(message.arguments[3], ' ');
  for (const std::string& nick: nicks)
    {
      IrcUser* user = channel->add_user(nick);
      if (user->nick != channel->get_self()->nick)
        {
          std::cout << "Adding user [" << nick << "] to chan " << chan_name << std::endl;
          this->bridge->send_user_join(this->hostname, chan_name, user->nick);
        }
    }
}

void IrcClient::on_self_channel_join(const IrcMessage& message)
{
  const std::string chan_name = message.arguments[0];
  IrcChannel* channel = this->get_channel(chan_name);
  channel->joined = true;
  channel->set_self(message.prefix);
}

void IrcClient::on_topic_received(const IrcMessage& message)
{
  const std::string chan_name = message.arguments[1];
  IrcChannel* channel = this->get_channel(chan_name);
  channel->topic = message.arguments[2];
}

void IrcClient::on_channel_completely_joined(const IrcMessage& message)
{
  const std::string chan_name = message.arguments[1];
  IrcChannel* channel = this->get_channel(chan_name);
  this->bridge->send_self_join(this->hostname, chan_name, channel->get_self()->nick);
  this->bridge->send_topic(this->hostname, chan_name, channel->topic);
}
