#include <utils/timed_events.hpp>
#include <irc/irc_message.hpp>
#include <irc/irc_client.hpp>
#include <bridge/bridge.hpp>
#include <irc/irc_user.hpp>

#include <logger/logger.hpp>
#include <utils/tolower.hpp>
#include <utils/split.hpp>

#include <iostream>
#include <stdexcept>

#include <chrono>
#include <string>

#include "config.h"

using namespace std::string_literals;
using namespace std::chrono_literals;

IrcClient::IrcClient(std::shared_ptr<Poller> poller, const std::string& hostname, const std::string& username, Bridge* bridge):
  TCPSocketHandler(poller),
  hostname(hostname),
  username(username),
  current_nick(username),
  bridge(bridge),
  welcomed(false),
  chanmodes({"", "", "", ""}),
  chantypes({'#', '&'})
{
  this->dummy_channel.topic = "This is a virtual channel provided for "
                              "convenience by biboumi, it is not connected "
                              "to any actual IRC channel of the server '" + this->hostname +
                              "', and sending messages in it has no effect. "
                              "Its main goal is to keep the connection to the IRC server "
                              "alive without having to join a real channel of that server. "
                              "To disconnect from the IRC server, leave this room and all "
                              "other IRC channels of that server.";
  // TODO: get the values from the preferences of the user, and only use the
  // list of default ports if the user didn't specify anything
  this->ports_to_try.emplace("6667", false); // standard non-encrypted port
#ifdef BOTAN_FOUND
  this->ports_to_try.emplace("6670", true);  // non-standard but I want it for some servers
  this->ports_to_try.emplace("6697", true);  // standard encrypted port
#endif // BOTAN_FOUND
}

IrcClient::~IrcClient()
{
  // This event may or may not exist (if we never got connected, it
  // doesn't), but it's ok
  TimedEventsManager::instance().cancel("PING"s + this->hostname + this->bridge->get_jid());
}

void IrcClient::start()
{
  if (this->connected || this->connecting)
    return ;
  std::string port;
  bool tls;
  std::tie(port, tls) = this->ports_to_try.top();
  this->ports_to_try.pop();
  this->bridge->send_xmpp_message(this->hostname, "", "Connecting to "s +
                                  this->hostname + ":" + port + " (" +
                                  (tls ? "encrypted" : "not encrypted") + ")");
  this->connect(this->hostname, port, tls);
}

void IrcClient::on_connection_failed(const std::string& reason)
{
  this->bridge->send_xmpp_message(this->hostname, "",
                                  "Connection failed: "s + reason);
  if (this->ports_to_try.empty())
    {
      // Send an error message for all room that the user wanted to join
      for (const auto& tuple: this->channels_to_join)
        {
          Iid iid(std::get<0>(tuple) + "%" + this->hostname);
          this->bridge->send_presence_error(iid, this->current_nick,
                                            "cancel", "item-not-found",
                                            "", reason);
        }
    }
  else                          // try the next port
    this->start();
}

void IrcClient::on_connected()
{
  this->send_nick_command(this->username);
  this->send_user_command(this->username, this->username);
  this->send_gateway_message("Connected to IRC server"s + (this->use_tls ? " (encrypted)": "") + ".");
  this->send_pending_data();
}

void IrcClient::on_connection_close(const std::string& error_msg)
{
  std::string message = "Connection closed by remote server.";
  if (!error_msg.empty())
    message += ": " + error_msg;
  const IrcMessage error{"ERROR", {message}};
  this->on_error(error);
  log_warning(message);
}

IrcChannel* IrcClient::get_channel(const std::string& n)
{
  if (n.empty())
    return &this->dummy_channel;
  const std::string name = utils::tolower(n);
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

void IrcClient::parse_in_buffer(const size_t)
{
  while (true)
    {
      auto pos = this->in_buf.find("\r\n");
      if (pos == std::string::npos)
        break ;
      IrcMessage message(this->in_buf.substr(0, pos));
      this->in_buf = this->in_buf.substr(pos + 2, std::string::npos);
      log_debug("IRC RECEIVING: " << message);

      // Call the standard callback (if any), associated with the command
      // name that we just received.
      auto cb = irc_callbacks.find(message.command);
      if (cb != irc_callbacks.end())
        {
          try {
            (this->*(cb->second))(message);
          } catch (const std::exception& e) {
            log_error("Unhandled exception: " << e.what());
          }
        }
      else
        log_info("No handler for command " << message.command);
      // Try to find a waiting_iq, which response will be triggered by this IrcMessage
      this->bridge->trigger_on_irc_message(this->hostname, message);
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

void IrcClient::send_quit_command(const std::string& reason)
{
  this->send_message(IrcMessage("QUIT", {reason}));
}

void IrcClient::send_join_command(const std::string& chan_name, const std::string& password)
{
  if (this->welcomed == false)
    this->channels_to_join.emplace_back(chan_name, password);
  else if (password.empty())
    this->send_message(IrcMessage("JOIN", {chan_name}));
  else
    this->send_message(IrcMessage("JOIN", {chan_name, password}));
  this->start();
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

void IrcClient::send_private_message(const std::string& username, const std::string& body, const std::string& type)
{
  std::string::size_type pos = 0;
  while (pos < body.size())
    {
      this->send_message(IrcMessage(std::string(type), {username, body.substr(pos, 400)}));
      pos += 400;
    }
  // We always try to insert and we don't care if the username was already
  // in the set.
  this->nicks_to_treat_as_private.insert(username);
}

void IrcClient::send_part_command(const std::string& chan_name, const std::string& status_message)
{
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->joined == true)
    {
      if (chan_name.empty())
        this->leave_dummy_channel(status_message);
      else
        this->send_message(IrcMessage("PART", {chan_name, status_message}));
    }
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

void IrcClient::send_ping_command()
{
  this->send_message(IrcMessage("PING", {"biboumi"}));
}

void IrcClient::forward_server_message(const IrcMessage& message)
{
  const std::string from = message.prefix;
  const std::string body = message.arguments[1];

  this->bridge->send_xmpp_message(this->hostname, from, body);
}

void IrcClient::on_notice(const IrcMessage& message)
{
  std::string from = message.prefix;
  const std::string to = message.arguments[0];
  const std::string body = message.arguments[1];

  if (!to.empty() && this->chantypes.find(to[0]) == this->chantypes.end())
    {
      // The notice is for the us precisely.

      // Find out if we already sent a private message to this user. If yes
      // we treat that message as a private message coming from
      // it. Otherwise we treat it as a notice coming from the server.
      IrcUser user(from);
      std::string nick = utils::tolower(user.nick);
      if (this->nicks_to_treat_as_private.find(nick) !=
          this->nicks_to_treat_as_private.end())
        { // We previously sent a message to that nick)
          this->bridge->send_message({nick + "!" + this->hostname}, nick, body,
                                     false);
        }
      else
        this->bridge->send_xmpp_message(this->hostname, from, body);
    }
  else
    {
      // The notice was directed at a channel we are in. Modify the message
      // to indicate that it is a notice, and make it a MUC message coming
      // from the MUC JID
      IrcMessage modified_message(std::move(from), "PRIVMSG", {to, "\u000303[notice]\u0003 "s + body});
      this->on_channel_message(modified_message);
    }
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
    else if (token.substr(0, 10) == "CHANTYPES=")
      {
        // Remove the default types, they apply only if no other value is
        // specified.
        this->chantypes.clear();
        size_t i = 10;
        while (i < token.size())
          this->chantypes.insert(token[i++]);
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
          this->bridge->send_user_join(this->hostname, chan_name, user,
                                       user->get_most_significant_mode(this->sorted_user_modes),
                                       false);
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
  IrcChannel* channel;
  if (chan_name.empty())
    channel = &this->dummy_channel;
  else
    channel = this->get_channel(chan_name);
  const std::string nick = message.prefix;
  if (channel->joined == false)
    channel->set_self(nick);
  else
    {
      const IrcUser* user = channel->add_user(nick, this->prefix_to_mode);
      this->bridge->send_user_join(this->hostname, chan_name, user,
                                   user->get_most_significant_mode(this->sorted_user_modes),
                                   false);
    }
}

void IrcClient::on_channel_message(const IrcMessage& message)
{
  const IrcUser user(message.prefix);
  const std::string nick = user.nick;
  Iid iid;
  iid.set_local(message.arguments[0]);
  iid.set_server(this->hostname);
  const std::string body = message.arguments[1];
  bool muc = true;
  if (!this->get_channel(iid.get_local())->joined)
    {
      iid.is_user = true;
      iid.set_local(nick);
      muc = false;
    }
  else
    iid.is_channel = true;
  if (!body.empty() && body[0] == '\01')
    {
      if (body.substr(1, 6) == "ACTION")
        this->bridge->send_message(iid, nick,
                  "/me"s + body.substr(7, body.size() - 8), muc);
      else if (body.substr(1, 8) == "VERSION\01")
        this->bridge->send_iq_version_request(nick, this->hostname);
      else if (body.substr(1, 5) == "PING ")
        this->bridge->send_xmpp_ping_request(nick, this->hostname,
                                             body.substr(6, body.size() - 7));
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
  if (message.arguments.size() < 2)
    return;
  const std::string chan_name = utils::tolower(message.arguments[message.arguments.size() - 2]);
  IrcChannel* channel = this->get_channel(chan_name);
  channel->topic = message.arguments[message.arguments.size() - 1];
  if (channel->joined)
    this->bridge->send_topic(this->hostname, chan_name, channel->topic);
}

void IrcClient::on_channel_completely_joined(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  channel->joined = true;
  this->bridge->send_user_join(this->hostname, chan_name, channel->get_self(),
                               channel->get_self()->get_most_significant_mode(this->sorted_user_modes),
                               true);
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
    iid.set_local(it->first);
    iid.set_server(this->hostname);
    iid.is_channel = true;
    this->bridge->send_nickname_conflict_error(iid, nickname);
  }
}

void IrcClient::on_nickname_change_too_fast(const IrcMessage& message)
{
  const std::string nickname = message.arguments[1];
  std::string txt;
  if (message.arguments.size() >= 3)
    txt = message.arguments[2];
  this->on_generic_error(message);
  for (auto it = this->channels.begin(); it != this->channels.end(); ++it)
  {
    Iid iid;
    iid.set_local(it->first);
    iid.set_server(this->hostname);
    iid.is_channel = true;
    this->bridge->send_presence_error(iid, nickname,
                                      "cancel", "not-acceptable",
                                      "", txt);
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
  // Install a repeated events to regularly send a PING
  TimedEventsManager::instance().add_event(TimedEvent(240s, std::bind(&IrcClient::send_ping_command, this),
                                                      "PING"s + this->hostname + this->bridge->get_jid()));
  for (const auto& tuple: this->channels_to_join)
    this->send_join_command(std::get<0>(tuple), std::get<1>(tuple));
  this->channels_to_join.clear();
  // Indicate that the dummy channel is joined as well, if needed
  if (this->dummy_channel.joining)
    {
      // Simulate a message coming from the IRC server saying that we joined
      // the channel
      const IrcMessage join_message(this->get_nick(), "JOIN", {""});
      this->on_channel_join(join_message);
      const IrcMessage end_join_message(std::string(this->hostname), "366",
                                        {this->get_nick(),
                                            "", "End of NAMES list"});
      this->on_channel_completely_joined(end_join_message);
    }
}

void IrcClient::on_part(const IrcMessage& message)
{
  const std::string chan_name = message.arguments[0];
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
      iid.set_local(chan_name);
      iid.set_server(this->hostname);
      iid.is_channel = true;
      bool self = channel->get_self()->nick == nick;
      if (self)
      {
        channel->joined = false;
        this->channels.erase(utils::tolower(chan_name));
        // channel pointer is now invalid
        channel = nullptr;
      }
      this->bridge->send_muc_leave(std::move(iid), std::move(nick), std::move(txt), self);
    }
}

void IrcClient::on_error(const IrcMessage& message)
{
  const std::string leave_message = message.arguments[0];
  // The user is out of all the channels
  for (auto it = this->channels.begin(); it != this->channels.end(); ++it)
  {
    Iid iid;
    iid.set_local(it->first);
    iid.set_server(this->hostname);
    iid.is_channel = true;
    IrcChannel* channel = it->second.get();
    if (!channel->joined)
      continue;
    std::string own_nick = channel->get_self()->nick;
    this->bridge->send_muc_leave(std::move(iid), std::move(own_nick), leave_message, true);
  }
  this->channels.clear();
  this->send_gateway_message("ERROR: "s + leave_message);
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
          iid.set_local(chan_name);
          iid.set_server(this->hostname);
          iid.is_channel = true;
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
          iid.set_local(chan_name);
          iid.set_server(this->hostname);
          iid.is_channel = true;
          const bool self = channel->get_self()->nick == old_nick;
          const char user_mode = user->get_most_significant_mode(this->sorted_user_modes);
          this->bridge->send_nick_change(std::move(iid), old_nick, new_nick, user_mode, self);
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
  iid.set_local(chan_name);
  iid.set_server(this->hostname);
  iid.is_channel = true;
  this->bridge->kick_muc_user(std::move(iid), target, reason, author.nick);
}

void IrcClient::on_mode(const IrcMessage& message)
{
  const std::string target = message.arguments[0];
  if (this->chantypes.find(target[0]) != this->chantypes.end())
    this->on_channel_mode(message);
  else
    this->on_user_mode(message);
}

void IrcClient::on_channel_mode(const IrcMessage& message)
{
  // For now, just transmit the modes so the user can know what happens
  // TODO, actually interprete the mode.
  Iid iid;
  iid.set_local(message.arguments[0]);
  iid.set_server(this->hostname);
  iid.is_channel = true;
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
  this->bridge->send_message(iid, "", "Mode "s + iid.get_local() +
                                      " [" + mode_arguments + "] by " + user.nick,
                             true);
  const IrcChannel* channel = this->get_channel(iid.get_local());
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
                              << "' in channel" << iid.get_local());
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
                                  "User mode for "s + message.arguments[0] +
                                  " is [" + message.arguments[1] + "]");
}

size_t IrcClient::number_of_joined_channels() const
{
  if (this->dummy_channel.joined)
    return this->channels.size() + 1;
  else
    return this->channels.size();
}

DummyIrcChannel& IrcClient::get_dummy_channel()
{
  return this->dummy_channel;
}

void IrcClient::leave_dummy_channel(const std::string& exit_message)
{
  if (!this->dummy_channel.joined)
    return;
  this->dummy_channel.joined = false;
  this->dummy_channel.joining = false;
  this->dummy_channel.remove_all_users();
  this->bridge->send_muc_leave(Iid("%"s + this->hostname), std::string(this->current_nick), exit_message, true);
}
