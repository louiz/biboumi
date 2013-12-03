#include <irc/irc_message.hpp>
#include <irc/irc_client.hpp>
#include <bridge/bridge.hpp>
#include <irc/irc_user.hpp>

#include <utils/make_unique.hpp>
#include <logger/logger.hpp>
#include <utils/tolower.hpp>
#include <utils/split.hpp>

#include <iostream>
#include <stdexcept>

IrcClient::IrcClient(const std::string& hostname, const std::string& username, Bridge* bridge):
  hostname(hostname),
  username(username),
  current_nick(username),
  bridge(bridge),
  welcomed(false)
{
}

IrcClient::~IrcClient()
{
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
  log_warning("Connection closed by remote server.");
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

bool IrcClient::is_channel_joined(const std::string& name)
{
  IrcChannel* client = this->get_channel(name);
  return client->joined;
}

std::string IrcClient::get_own_nick() const
{
  return this->current_nick;
}

void IrcClient::parse_in_buffer()
{
  while (true)
    {
      auto pos = this->in_buf.find("\r\n");
      if (pos == std::string::npos)
        break ;
      IrcMessage message(this->in_buf.substr(0, pos));
      log_debug("IRC RECEIVING: " << message);
      this->in_buf = this->in_buf.substr(pos + 2, std::string::npos);
      auto cb = irc_callbacks.find(message.command);
      if (cb != irc_callbacks.end())
        (this->*(cb->second))(message);
      else
        log_info("No handler for command " << message.command);
    }
}

void IrcClient::send_message(IrcMessage&& message)
{
  log_debug("IRC SENDING: " << message);
  std::string res;
  if (!message.prefix.empty())
    res += ":" + std::move(message.prefix) + " ";
  res += std::move(message.command);
  for (const std::string& arg: message.arguments)
    {
      if (arg.find(" ") != std::string::npos ||
          (!arg.empty() && arg[0] == ':'))
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
  this->send_message(IrcMessage("USER", {username, "ignored", "ignored", realname}));
}

void IrcClient::send_nick_command(const std::string& nick)
{
  this->send_message(IrcMessage("NICK", {nick}));
}

void IrcClient::send_kick_command(const std::string& chan_name, const std::string& target, const std::string& reason)
{
  this->send_message(IrcMessage("KICK", {chan_name, target, reason}));
}

void IrcClient::send_join_command(const std::string& chan_name)
{
  if (this->welcomed == false)
    this->channels_to_join.push_back(chan_name);
  else
    this->send_message(IrcMessage("JOIN", {chan_name}));
}

bool IrcClient::send_channel_message(const std::string& chan_name, const std::string& body)
{
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->joined == false)
    {
      log_warning("Cannot send message to channel " << chan_name << ", it is not joined");
      return false;
    }
  this->send_message(IrcMessage("PRIVMSG", {chan_name, body}));
  return true;
}

void IrcClient::send_private_message(const std::string& username, const std::string& body)
{
  this->send_message(IrcMessage("PRIVMSG", {username, body}));
}

void IrcClient::send_part_command(const std::string& chan_name, const std::string& status_message)
{
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->joined == true)
    this->send_message(IrcMessage("PART", {chan_name, status_message}));
}

void IrcClient::send_mode_command(const std::string& chan_name, const std::vector<std::string>& arguments)
{
  std::vector<std::string> args(arguments);
  args.insert(args.begin(), chan_name);
  IrcMessage m("MODE", std::move(args));
  this->send_message(std::move(m));
}

void IrcClient::send_pong_command(const IrcMessage& message)
{
  const std::string id = message.arguments[0];
  this->send_message(IrcMessage("PONG", {id}));
}

void IrcClient::forward_server_message(const IrcMessage& message)
{
  const std::string from = message.prefix;
  const std::string body = message.arguments[1];

  this->bridge->send_xmpp_message(this->hostname, from, body);
}

void IrcClient::set_and_forward_user_list(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[2]);
  IrcChannel* channel = this->get_channel(chan_name);
  std::vector<std::string> nicks = utils::split(message.arguments[3], ' ');
  for (const std::string& nick: nicks)
    {
      IrcUser* user = channel->add_user(nick);
      if (user->nick != channel->get_self()->nick)
        {
          log_debug("Adding user [" << nick << "] to chan " << chan_name);
          this->bridge->send_user_join(this->hostname, chan_name, user->nick);
        }
    }
}

void IrcClient::on_channel_join(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[0]);
  IrcChannel* channel = this->get_channel(chan_name);
  const std::string nick = message.prefix;
  if (channel->joined == false)
    {
      channel->joined = true;
      channel->set_self(nick);
    }
  else
    {
      IrcUser* user = channel->add_user(nick);
      this->bridge->send_user_join(this->hostname, chan_name, user->nick);
    }
}

void IrcClient::on_channel_message(const IrcMessage& message)
{
  const IrcUser user(message.prefix);
  const std::string nick = user.nick;
  Iid iid;
  iid.chan = message.arguments[0];
  iid.server = this->hostname;
  const std::string body = message.arguments[1];
  bool muc = true;
  if (!this->get_channel(iid.chan)->joined)
    {
      iid.chan = nick;
      muc = false;
    }
  if (!body.empty() && body[0] == '\01')
    {
      if (body.substr(1, 6) == "ACTION")
        this->bridge->send_message(iid, nick,
                  std::string("/me") + body.substr(7, body.size() - 8), muc);
    }
  else
    this->bridge->send_message(iid, nick, body, muc);
}

void IrcClient::empty_motd(const IrcMessage& message)
{
  (void)message;
  this->motd.erase();
}

void IrcClient::on_motd_line(const IrcMessage& message)
{
  const std::string body = message.arguments[1];
  // We could send the MOTD without a line break between each IRC-message,
  // but sometimes it contains some ASCII art, we use line breaks to keep
  // them intact.
  this->motd += body+"\n";
}

void IrcClient::send_motd(const IrcMessage& message)
{
  (void)message;
  this->bridge->send_xmpp_message(this->hostname, "", this->motd);
}

void IrcClient::on_topic_received(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  channel->topic = message.arguments[2];
}

void IrcClient::on_channel_completely_joined(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  this->bridge->send_self_join(this->hostname, chan_name, channel->get_self()->nick);
  this->bridge->send_topic(this->hostname, chan_name, channel->topic);
}

void IrcClient::on_welcome_message(const IrcMessage& message)
{
  this->current_nick = message.arguments[0];
  this->welcomed = true;
  for (const std::string& chan_name: this->channels_to_join)
    this->send_join_command(chan_name);
  this->channels_to_join.clear();
}

void IrcClient::on_part(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[0]);
  IrcChannel* channel = this->get_channel(chan_name);
  std::string txt;
  if (message.arguments.size() >= 2)
    txt = message.arguments[1];
  const IrcUser* user = channel->find_user(message.prefix);
  if (user)
    {
      std::string nick = user->nick;
      channel->remove_user(user);
      Iid iid;
      iid.chan = chan_name;
      iid.server = this->hostname;
      bool self = channel->get_self()->nick == nick;
      this->bridge->send_muc_leave(std::move(iid), std::move(nick), std::move(txt), self);
      if (self)
        channel->joined = false;
    }
}

void IrcClient::on_quit(const IrcMessage& message)
{
  std::string txt;
  if (message.arguments.size() >= 1)
    txt = message.arguments[0];
  for (auto it = this->channels.begin(); it != this->channels.end(); ++it)
    {
      const std::string chan_name = it->first;
      IrcChannel* channel = it->second.get();
      const IrcUser* user = channel->find_user(message.prefix);
      if (user)
        {
          std::string nick = user->nick;
          channel->remove_user(user);
          Iid iid;
          iid.chan = chan_name;
          iid.server = this->hostname;
          this->bridge->send_muc_leave(std::move(iid), std::move(nick), txt, false);
        }
    }
}

void IrcClient::on_nick(const IrcMessage& message)
{
  const std::string new_nick = message.arguments[0];
  for (auto it = this->channels.begin(); it != this->channels.end(); ++it)
    {
      const std::string chan_name = it->first;
      IrcChannel* channel = it->second.get();
      IrcUser* user = channel->find_user(message.prefix);
      if (user)
        {
          std::string old_nick = user->nick;
          Iid iid;
          iid.chan = chan_name;
          iid.server = this->hostname;
          bool self = channel->get_self()->nick == old_nick;
          this->bridge->send_nick_change(std::move(iid), old_nick, new_nick, self);
          user->nick = new_nick;
          if (self)
            {
              channel->get_self()->nick = new_nick;
              this->current_nick = new_nick;
            }
        }
    }
}

void IrcClient::on_kick(const IrcMessage& message)
{
  const std::string target = message.arguments[1];
  const std::string reason = message.arguments[2];
  const std::string chan_name = utils::tolower(message.arguments[0]);
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->get_self()->nick == target)
    channel->joined = false;
  IrcUser author(message.prefix);
  Iid iid;
  iid.chan = chan_name;
  iid.server = this->hostname;
  this->bridge->kick_muc_user(std::move(iid), target, reason, author.nick);
}

void IrcClient::on_mode(const IrcMessage& message)
{
  const std::string target = message.arguments[0];
  if (target[0] == '&' || target[0] == '#' ||
      target[0] == '!' || target[0] == '+')
    this->on_channel_mode(message);
  else
    this->on_user_mode(message);
}

void IrcClient::on_channel_mode(const IrcMessage& message)
{
  // For now, just transmit the modes so the user can know what happens
  // TODO, actually interprete the mode.
  Iid iid;
  iid.chan = message.arguments[0];
  iid.server = this->hostname;
  IrcUser user(message.prefix);
  this->bridge->send_message(iid, "", std::string("Mode ") + iid.chan +
                                      " [" + message.arguments[1] +
                                      (message.arguments.size() > 2 ? (" " + message.arguments[2]): "")
                                      + "] by " + user.nick,
                             true);
}

void IrcClient::on_user_mode(const IrcMessage& message)
{
  this->bridge->send_xmpp_message(this->hostname, "",
                                  std::string("User mode for ") + message.arguments[0] +
                                  " is [" + message.arguments[1] + "]");
}
