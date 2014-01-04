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
  welcomed(false),
  chanmodes({"", "", "", ""})
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
  this->send_gateway_message("Connected to IRC server.");
}

void IrcClient::on_connection_close()
{
  static const std::string message = "Connection closed by remote server.";
  this->send_gateway_message(message);
  log_warning(message);
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
  IrcChannel* channel = this->get_channel(name);
  return channel->joined;
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

void IrcClient::send_topic_command(const std::string& chan_name, const std::string& topic)
{
  this->send_message(IrcMessage("TOPIC", {chan_name, topic}));
}

void IrcClient::send_quit_command()
{
  this->send_message(IrcMessage("QUIT", {"gateway shutdown"}));
}

void IrcClient::send_join_command(const std::string& chan_name)
{
  if (!this->connected)
    this->start();
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
  // Cut the message body into 400-bytes parts (because the whole command
  // must fit into 512 bytes, that's an easy way to make sure the chan name
  // + body fits. I’m lazy.)
  std::string::size_type pos = 0;
  while (pos < body.size())
    {
      this->send_message(IrcMessage("PRIVMSG", {chan_name, body.substr(pos, 400)}));
      pos += 400;
    }
  return true;
}

void IrcClient::send_private_message(const std::string& username, const std::string& body)
{
  std::string::size_type pos = 0;
  while (pos < body.size())
    {
      this->send_message(IrcMessage("PRIVMSG", {username, body.substr(pos, 400)}));
      pos += 400;
    }

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

void IrcClient::on_isupport_message(const IrcMessage& message)
{
  const size_t len = message.arguments.size();
  for (size_t i = 1; i < len; ++i)
  {
    const std::string token = message.arguments[i];
    if (token.substr(0, 10) == "CHANMODES=")
    {
      this->chanmodes = utils::split(token.substr(11), ',');
      // make sure we have 4 strings
      this->chanmodes.resize(4);
    }
    else if (token.substr(0, 7) == "PREFIX=")
      {
        size_t i = 8;           // jump PREFIX=(
        size_t j = 9;
        // Find the ) char
        while (j < token.size() && token[j] != ')')
          j++;
        j++;
        while (j < token.size() && token[i] != ')')
          {
            this->sorted_user_modes.push_back(token[i]);
            this->prefix_to_mode[token[j++]] = token[i++];
          }
      }
  }
}

void IrcClient::send_gateway_message(const std::string& message, const std::string& from)
{
  this->bridge->send_xmpp_message(this->hostname, from, message);
}

void IrcClient::set_and_forward_user_list(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[2]);
  IrcChannel* channel = this->get_channel(chan_name);
  std::vector<std::string> nicks = utils::split(message.arguments[3], ' ');
  for (const std::string& nick: nicks)
    {
      const IrcUser* user = channel->add_user(nick, this->prefix_to_mode);
      if (user->nick != channel->get_self()->nick)
        {
          log_debug("Adding user [" << nick << "] to chan " << chan_name);
          this->bridge->send_user_join(this->hostname, chan_name, user, false);
        }
      else
        {
          // we now know the modes of self, so copy the modes into self
          channel->get_self()->modes = user->modes;
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
      const IrcUser* user = channel->add_user(nick, this->prefix_to_mode);
      this->bridge->send_user_join(this->hostname, chan_name, user, false);
    }
}

void IrcClient::on_channel_message(const IrcMessage& message)
{
  const IrcUser user(message.prefix);
  const std::string nick = user.nick;
  Iid iid;
  iid.chan = utils::tolower(message.arguments[0]);
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
  if (channel->joined)
    this->bridge->send_topic(this->hostname, chan_name, channel->topic);
}

void IrcClient::on_channel_completely_joined(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  this->bridge->send_user_join(this->hostname, chan_name, channel->get_self(), true);
  this->bridge->send_topic(this->hostname, chan_name, channel->topic);
}

void IrcClient::on_erroneous_nickname(const IrcMessage& message)
{
  const std::string error_msg = message.arguments.size() >= 3 ?
    message.arguments[2]: "Erroneous nickname";
  this->send_gateway_message(error_msg + ": " + message.arguments[1], message.prefix);
}

void IrcClient::on_nickname_conflict(const IrcMessage& message)
{
  const std::string nickname = message.arguments[1];
  this->on_generic_error(message);
  for (auto it = this->channels.begin(); it != this->channels.end(); ++it)
  {
    Iid iid;
    iid.chan = it->first;
    iid.server = this->hostname;
    this->bridge->send_nickname_conflict_error(iid, nickname);
  }
}

void IrcClient::on_generic_error(const IrcMessage& message)
{
  const std::string error_msg = message.arguments.size() >= 3 ?
    message.arguments[2]: "Unspecified error";
  this->send_gateway_message(message.arguments[1] + ": " + error_msg, message.prefix);
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
  if (!channel->joined)
    return ;
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
      {
        channel->joined = false;
        this->channels.erase(chan_name);
        // channel pointer is now invalid
        channel = nullptr;
      }
    }
}

void IrcClient::on_error(const IrcMessage& message)
{
  const std::string leave_message = message.arguments[0];
  // The user is out of all the channels
  for (auto it = this->channels.begin(); it != this->channels.end(); ++it)
  {
    Iid iid;
    iid.chan = it->first;
    iid.server = this->hostname;
    IrcChannel* channel = it->second.get();
    if (!channel->joined)
      continue;
    std::string own_nick = channel->get_self()->nick;
    this->bridge->send_muc_leave(std::move(iid), std::move(own_nick), leave_message, true);
  }
  this->channels.clear();
  this->send_gateway_message(std::string("ERROR: ") + leave_message);
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
  if (!channel->joined)
    return ;
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
  iid.chan = utils::tolower(message.arguments[0]);
  iid.server = this->hostname;
  IrcUser user(message.prefix);
  std::string mode_arguments;
  for (size_t i = 1; i < message.arguments.size(); ++i)
    {
      if (!message.arguments[i].empty())
        {
          if (i != 1)
            mode_arguments += " ";
          mode_arguments += message.arguments[i];
        }
    }
  this->bridge->send_message(iid, "", std::string("Mode ") + iid.chan +
                                      " [" + mode_arguments + "] by " + user.nick,
                             true);
  const IrcChannel* channel = this->get_channel(iid.chan);
  if (!channel)
    return;

  // parse the received modes, we need to handle things like "+m-oo coucou toutou"
  const std::string modes = message.arguments[1];
  // a list of modified IrcUsers. When we applied all modes, we check the
  // modes that now applies to each of them, and send a notification for
  // each one. This is to disallow sending two notifications or more when a
  // single MODE command changes two or more modes on the same participant
  std::set<const IrcUser*> modified_users;
  // If it is true, the modes are added, if it’s false they are
  // removed. When we encounter the '+' char, the value is changed to true,
  // and with '-' it is changed to false.
  bool add = true;
  bool use_arg;
  size_t arg_pos = 2;
  for (const char c: modes)
    {
      if (c == '+')
        add = true;
      else if (c == '-')
        add = false;
      else
        { // lookup the mode symbol in the 4 chanmodes lists, depending on
          // the list where it is found, it takes an argument or not
          size_t type;
          for (type = 0; type < 4; ++type)
            if (this->chanmodes[type].find(c) != std::string::npos)
              break;
          if (type == 4)        // if mode was not found
            {
              // That mode can also be of type B if it is present in the
              // prefix_to_mode map
              for (const std::pair<char, char>& pair: this->prefix_to_mode)
                if (pair.second == c)
                  {
                    type = 1;
                    break;
                  }
            }
          // modes of type A, B or C (but only with add == true)
          if (type == 0 || type == 1 ||
              (type == 2 && add == true))
            use_arg = true;
          else // modes of type C (but only with add == false), D, or unknown
            use_arg = false;
          if (use_arg == true && message.arguments.size() > arg_pos)
            {
              const std::string target = message.arguments[arg_pos++];
              IrcUser* user = channel->find_user(target);
              if (!user)
                {
                  log_warning("Trying to set mode for non-existing user '" << target
                              << "' in channel" << iid.chan);
                  return;
                }
              if (add)
                user->add_mode(c);
              else
                user->remove_mode(c);
              modified_users.insert(user);
            }
        }
    }
  for (const IrcUser* u: modified_users)
    {
      char most_significant_mode = u->get_most_significant_mode(this->sorted_user_modes);
      this->bridge->send_affiliation_role_change(iid, u->nick, most_significant_mode);
    }
}

void IrcClient::on_user_mode(const IrcMessage& message)
{
  this->bridge->send_xmpp_message(this->hostname, "",
                                  std::string("User mode for ") + message.arguments[0] +
                                  " is [" + message.arguments[1] + "]");
}
