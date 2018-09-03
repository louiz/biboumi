#include <utility>
#include <utils/timed_events.hpp>
#include <database/database.hpp>
#include <irc/irc_message.hpp>
#include <irc/irc_client.hpp>
#include <bridge/bridge.hpp>
#include <irc/irc_user.hpp>

#include <logger/logger.hpp>
#include <config/config.hpp>
#include <utils/tolower.hpp>
#include <utils/split.hpp>
#include <utils/string.hpp>

#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>

#include <chrono>
#include <string>

#include "biboumi.h"

using namespace std::string_literals;
using namespace std::chrono_literals;

/**
 * Define a map of functions to be called for each IRC command we can
 * handle.
 */
using IrcCallback = void (IrcClient::*)(const IrcMessage&);

static const std::unordered_map<std::string,
                                std::pair<IrcCallback, std::pair<std::size_t, std::size_t>>> irc_callbacks = {
  {"NOTICE", {&IrcClient::on_notice, {2, 0}}},
  {"002", {&IrcClient::forward_server_message, {2, 0}}},
  {"003", {&IrcClient::forward_server_message, {2, 0}}},
  {"004", {&IrcClient::on_server_myinfo, {4, 0}}},
  {"005", {&IrcClient::on_isupport_message, {0, 0}}},
  {"RPL_LISTSTART", {&IrcClient::on_rpl_liststart, {0, 0}}},
  {"321", {&IrcClient::on_rpl_liststart, {0, 0}}},
  {"RPL_LIST", {&IrcClient::on_rpl_list, {0, 0}}},
  {"322", {&IrcClient::on_rpl_list, {0, 0}}},
  {"RPL_LISTEND", {&IrcClient::on_rpl_listend, {0, 0}}},
  {"323", {&IrcClient::on_rpl_listend, {0, 0}}},
  {"RPL_NOTOPIC", {&IrcClient::on_empty_topic, {0, 0}}},
  {"331", {&IrcClient::on_empty_topic, {0, 0}}},
  {"341", {&IrcClient::on_invited, {3, 0}}},
  {"RPL_MOTDSTART", {&IrcClient::empty_motd, {0, 0}}},
  {"375", {&IrcClient::empty_motd, {0, 0}}},
  {"RPL_MOTD", {&IrcClient::on_motd_line, {2, 0}}},
  {"372", {&IrcClient::on_motd_line, {2, 0}}},
  {"RPL_MOTDEND", {&IrcClient::send_motd, {0, 0}}},
  {"376", {&IrcClient::send_motd, {0, 0}}},
  {"JOIN", {&IrcClient::on_channel_join, {1, 0}}},
  {"PRIVMSG", {&IrcClient::on_channel_message, {2, 0}}},
  {"353", {&IrcClient::set_and_forward_user_list, {4, 0}}},
  {"332", {&IrcClient::on_topic_received, {2, 0}}},
  {"TOPIC", {&IrcClient::on_topic_received, {2, 0}}},
  {"333", {&IrcClient::on_topic_who_time_received, {4, 0}}},
  {"RPL_TOPICWHOTIME", {&IrcClient::on_topic_who_time_received, {4, 0}}},
  {"366", {&IrcClient::on_channel_completely_joined, {2, 0}}},
  {"367", {&IrcClient::on_banlist, {3, 0}}},
  {"368", {&IrcClient::on_banlist_end, {3, 0}}},
  {"396", {&IrcClient::on_own_host_received, {2, 0}}},
  {"432", {&IrcClient::on_erroneous_nickname, {2, 0}}},
  {"433", {&IrcClient::on_nickname_conflict, {2, 0}}},
  {"438", {&IrcClient::on_nickname_change_too_fast, {2, 0}}},
  {"443", {&IrcClient::on_useronchannel, {3, 0}}},
  {"475", {&IrcClient::on_channel_bad_key, {3, 0}}},
  {"ERR_USERONCHANNEL", {&IrcClient::on_useronchannel, {3, 0}}},
  {"001", {&IrcClient::on_welcome_message, {1, 0}}},
  {"PART", {&IrcClient::on_part, {1, 0}}},
  {"ERROR", {&IrcClient::on_error, {1, 0}}},
  {"QUIT", {&IrcClient::on_quit, {0, 0}}},
  {"NICK", {&IrcClient::on_nick, {1, 0}}},
  {"MODE", {&IrcClient::on_mode, {1, 0}}},
  {"PING", {&IrcClient::send_pong_command, {1, 0}}},
  {"PONG", {&IrcClient::on_pong, {0, 0}}},
  {"KICK", {&IrcClient::on_kick, {3, 0}}},
  {"INVITE", {&IrcClient::on_invite, {2, 0}}},

  {"401", {&IrcClient::on_generic_error, {2, 0}}},
  {"402", {&IrcClient::on_generic_error, {2, 0}}},
  {"403", {&IrcClient::on_generic_error, {2, 0}}},
  {"404", {&IrcClient::on_generic_error, {2, 0}}},
  {"405", {&IrcClient::on_generic_error, {2, 0}}},
  {"406", {&IrcClient::on_generic_error, {2, 0}}},
  {"407", {&IrcClient::on_generic_error, {2, 0}}},
  {"408", {&IrcClient::on_generic_error, {2, 0}}},
  {"409", {&IrcClient::on_generic_error, {2, 0}}},
  {"410", {&IrcClient::on_generic_error, {2, 0}}},
  {"411", {&IrcClient::on_generic_error, {2, 0}}},
  {"412", {&IrcClient::on_generic_error, {2, 0}}},
  {"414", {&IrcClient::on_generic_error, {2, 0}}},
  {"421", {&IrcClient::on_generic_error, {2, 0}}},
  {"422", {&IrcClient::on_generic_error, {2, 0}}},
  {"423", {&IrcClient::on_generic_error, {2, 0}}},
  {"424", {&IrcClient::on_generic_error, {2, 0}}},
  {"431", {&IrcClient::on_generic_error, {2, 0}}},
  {"436", {&IrcClient::on_generic_error, {2, 0}}},
  {"441", {&IrcClient::on_generic_error, {2, 0}}},
  {"442", {&IrcClient::on_generic_error, {2, 0}}},
  {"444", {&IrcClient::on_generic_error, {2, 0}}},
  {"446", {&IrcClient::on_generic_error, {2, 0}}},
  {"451", {&IrcClient::on_generic_error, {2, 0}}},
  {"461", {&IrcClient::on_generic_error, {2, 0}}},
  {"462", {&IrcClient::on_generic_error, {2, 0}}},
  {"463", {&IrcClient::on_generic_error, {2, 0}}},
  {"464", {&IrcClient::on_generic_error, {2, 0}}},
  {"465", {&IrcClient::on_generic_error, {2, 0}}},
  {"467", {&IrcClient::on_generic_error, {2, 0}}},
  {"470", {&IrcClient::on_generic_error, {2, 0}}},
  {"471", {&IrcClient::on_generic_error, {2, 0}}},
  {"472", {&IrcClient::on_generic_error, {2, 0}}},
  {"473", {&IrcClient::on_generic_error, {2, 0}}},
  {"474", {&IrcClient::on_generic_error, {2, 0}}},
  {"476", {&IrcClient::on_generic_error, {2, 0}}},
  {"477", {&IrcClient::on_generic_error, {2, 0}}},
  {"481", {&IrcClient::on_generic_error, {2, 0}}},
  {"482", {&IrcClient::on_generic_error, {2, 0}}},
  {"483", {&IrcClient::on_generic_error, {2, 0}}},
  {"484", {&IrcClient::on_generic_error, {2, 0}}},
  {"485", {&IrcClient::on_generic_error, {2, 0}}},
  {"487", {&IrcClient::on_generic_error, {2, 0}}},
  {"491", {&IrcClient::on_generic_error, {2, 0}}},
  {"501", {&IrcClient::on_generic_error, {2, 0}}},
  {"502", {&IrcClient::on_generic_error, {2, 0}}},
};

IrcClient::IrcClient(std::shared_ptr<Poller>& poller, std::string hostname,
                     std::string nickname, std::string username,
                     std::string realname, std::string user_hostname,
                     Bridge& bridge):
  TCPClientSocketHandler(poller),
  hostname(hostname),
  user_hostname(std::move(user_hostname)),
  username(std::move(username)),
  realname(std::move(realname)),
  current_nick(std::move(nickname)),
  bridge(bridge),
  welcomed(false),
  chanmodes({"", "", "", ""}),
  chantypes({'#', '&'}),
  tokens_bucket(this->get_throttle_limit(), 1s, [this]() {
    if (message_queue.empty())
      return true;
    this->actual_send(std::move(this->message_queue.front()));
    this->message_queue.pop_front();
    return false;
  }, "TokensBucket" + this->hostname + this->bridge.get_jid())
{
#ifdef USE_DATABASE
  auto options = Database::get_irc_server_options(this->bridge.get_bare_jid(),
                                                  this->get_hostname());
  std::vector<std::string> ports = utils::split(options.col<Database::Ports>(), ';', false);
  for (auto it = ports.rbegin(); it != ports.rend(); ++it)
    this->ports_to_try.emplace(*it, false);
# ifdef BOTAN_FOUND
  ports = utils::split(options.col<Database::TlsPorts>(), ';', false);
  for (auto it = ports.rbegin(); it != ports.rend(); ++it)
    this->ports_to_try.emplace(*it, true);
# endif // BOTAN_FOUND

#else  // not USE_DATABASE
  this->ports_to_try.emplace("6667", false); // standard non-encrypted port
# ifdef BOTAN_FOUND
  this->ports_to_try.emplace("6670", true);  // non-standard but I want it for some servers
  this->ports_to_try.emplace("6697", true);  // standard encrypted port
# endif // BOTAN_FOUND
#endif // USE_DATABASE
}

IrcClient::~IrcClient()
{
  // This event may or may not exist (if we never got connected, it
  // doesn't), but it's ok
  TimedEventsManager::instance().cancel("PING" + this->hostname + this->bridge.get_jid());
  TimedEventsManager::instance().cancel("TokensBucket" + this->hostname + this->bridge.get_jid());
}

void IrcClient::start()
{
  if (this->is_connecting() || this->is_connected())
    return;
  if (this->ports_to_try.empty())
    {
      this->bridge.send_xmpp_message(this->hostname, "", "Can not connect to IRC server: no port specified.");
      return;
    }
  std::string port;
  bool tls;
  std::tie(port, tls) = this->ports_to_try.top();
  this->ports_to_try.pop();
  this->bind_addr = Config::get("outgoing_bind", "");
  std::string address = this->hostname;

#ifdef USE_DATABASE
  auto options = Database::get_irc_server_options(this->bridge.get_bare_jid(),
                                                  this->get_hostname());
# ifdef BOTAN_FOUND
  this->credential_manager.set_trusted_fingerprint(options.col<Database::TrustedFingerprint>());
# endif
  if (Config::get("fixed_irc_server", "").empty() &&
      !options.col<Database::Address>().empty())
    address = options.col<Database::Address>();
#endif
  this->bridge.send_xmpp_message(this->hostname, "", "Connecting to " +
                                  address + ":" + port + " (" +
                                  (tls ? "encrypted" : "not encrypted") + ")");
  this->connect(address, port, tls);
}

void IrcClient::on_connection_failed(const std::string& reason)
{
  this->bridge.send_xmpp_message(this->hostname, "",
                                  "Connection failed: " + reason);

  if (this->hostname_resolution_failed)
    while (!this->ports_to_try.empty())
      this->ports_to_try.pop();

  if (this->ports_to_try.empty())
    {
      // Send an error message for all room that the user wanted to join
      for (const auto& tuple: this->channels_to_join)
        {
          Iid iid(std::get<0>(tuple) + "%" + this->hostname, this->chantypes);
          this->bridge.send_presence_error(iid, this->current_nick,
                                            "cancel", "item-not-found",
                                            "", reason);
        }
    }
  else                          // try the next port
    this->start();
}

void IrcClient::on_connected()
{
  const auto webirc_password = Config::get("webirc_password", "");
  static std::string resolved_ip;

  if (!webirc_password.empty())
    {
      if (!resolved_ip.empty())
        this->send_webirc_command(webirc_password, resolved_ip);
      else
        {  // Start resolving the hostname of the user, and call
           // on_connected again when it’s done
          this->dns_resolver.resolve(this->user_hostname, "5222",
                                     [this](const struct addrinfo* addr)
                                     {
                                       resolved_ip = addr_to_string(addr);
                                       // Only continue the process if we
                                       // didn’t get connected while we were
                                       // resolving
                                       if (this->is_connected())
                                         this->on_connected();
                                     },
                                     [this](const char* error_msg)
                                     {
                                       if (this->is_connected())
                                         {
                                           this->on_connection_close("Could not resolve hostname " + this->user_hostname +
                                                                     ": " + error_msg);
                                           this->send_quit_command("");
                                         }
                                     });
          return;
        }
    }

  this->send_message({"CAP", {"REQ", "multi-prefix"}});
  this->send_message({"CAP", {"END"}});

#ifdef USE_DATABASE
  auto options = Database::get_irc_server_options(this->bridge.get_bare_jid(),
                                                  this->get_hostname());
  if (!options.col<Database::Pass>().empty())
    this->send_pass_command(options.col<Database::Pass>());
#endif

  this->send_nick_command(this->current_nick);

#ifdef USE_DATABASE
  if (Config::get("realname_customization", "true") == "true")
    {
      if (!options.col<Database::Username>().empty())
        this->username = options.col<Database::Username>();
      if (!options.col<Database::Realname>().empty())
        this->realname = options.col<Database::Realname>();
      this->send_user_command(username, realname);
    }
  else
    this->send_user_command(this->username, this->realname);
#else
  this->send_user_command(this->username, this->realname);
#endif
  this->send_gateway_message("Connected to IRC server"s + (this->use_tls ? " (encrypted)": "") + ".");
  this->send_pending_data();
  this->bridge.on_irc_client_connected(this->get_hostname());
}

void IrcClient::on_connection_close(const std::string& error_msg)
{
  std::string message = "Connection closed";
  if (!error_msg.empty())
    message += ": " + error_msg;
  else
    message += ".";
  const IrcMessage error{"ERROR", {message}};
  this->on_error(error);
  log_warning(message);
  this->bridge.on_irc_client_disconnected(this->get_hostname());
}

IrcChannel* IrcClient::get_channel(const std::string& n)
{
  const std::string name = utils::tolower(n);
  try
    {
      return this->channels.at(name).get();
    }
  catch (const std::out_of_range& exception)
    {
      return this->channels.emplace(name, std::make_unique<IrcChannel>()).first->second.get();
    }
}

const IrcChannel* IrcClient::find_channel(const std::string& n) const
{
  const std::string name = utils::tolower(n);
  try
    {
      return this->channels.at(name).get();
    }
  catch (const std::out_of_range& exception)
    {
      return nullptr;
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
      this->consume_in_buffer(pos + 2);
      log_debug("IRC RECEIVING: (", this->get_hostname(), ") ", message);

      // Call the standard callback (if any), associated with the command
      // name that we just received.
      auto it = irc_callbacks.find(message.command);
      if (it != irc_callbacks.end())
        {
          const auto& limits = it->second.second;
          // Check that the Message is well formed before actually calling
          // the callback. limits.first is the min number of arguments,
          // second is the max
          if (message.arguments.size() < limits.first ||
              (limits.second > 0 && message.arguments.size() > limits.second))
            log_warning("Invalid number of arguments for IRC command “", message.command,
                        "”: ", message.arguments.size());
          else
            {
              const auto& cb = it->second.first;
              try {
                (this->*(cb))(message);
              } catch (const std::exception& e) {
                log_error("Unhandled exception: ", e.what());
              }
            }
        }
      else
        {
          log_info("No handler for command ", message.command,
                   ", forwarding the arguments to the user");
          this->on_unknown_message(message);
        }
      // Try to find a waiting_iq, which response will be triggered by this IrcMessage
      this->bridge.trigger_on_irc_message(this->hostname, message);
    }
}

void IrcClient::actual_send(std::pair<IrcMessage, MessageCallback> message_pair)
{
  const IrcMessage& message = message_pair.first;
  const MessageCallback& callback = message_pair.second;
   log_debug("IRC SENDING: (", this->get_hostname(), ") ", message);
    std::string res;
    if (!message.prefix.empty())
      res += ":" + message.prefix + " ";
    res += message.command;
    for (const std::string& arg: message.arguments)
      {
        if (arg.find(' ') != std::string::npos
            || (!arg.empty() && arg[0] == ':'))
          {
            res += " :" + arg;
            break;
          }
        res += " " + arg;
      }
    res += "\r\n";
    this->send_data(std::move(res));

    if (callback)
      callback(this, message);
 }

void IrcClient::send_message(IrcMessage message, MessageCallback callback, bool throttle)
{
  auto message_pair = std::make_pair(std::move(message), std::move(callback));
  if (this->tokens_bucket.use_token() || !throttle)
    this->actual_send(std::move(message_pair));
  else
    message_queue.push_back(std::move(message_pair));
}

void IrcClient::send_raw(const std::string& txt)
{
  log_debug("IRC SENDING (raw): (", this->get_hostname(), ") ", txt);
  this->send_data(txt + "\r\n");
}

void IrcClient::send_user_command(const std::string& username, const std::string& realname)
{
  this->send_message(IrcMessage("USER", {username, this->user_hostname, "ignored", realname}));
}

void IrcClient::send_nick_command(const std::string& nick)
{
  this->send_message(IrcMessage("NICK", {nick}));
}

void IrcClient::send_pass_command(const std::string& password)
{
  this->send_message(IrcMessage("PASS", {password}));
}

void IrcClient::send_webirc_command(const std::string& password, const std::string& user_ip)
{
  this->send_message(IrcMessage("WEBIRC", {password, "biboumi", this->user_hostname, user_ip}));
}

void IrcClient::send_kick_command(const std::string& chan_name, const std::string& target, const std::string& reason)
{
  this->send_message(IrcMessage("KICK", {chan_name, target, reason}));
}

void IrcClient::send_list_command()
{
  this->send_message(IrcMessage("LIST", {"*"}));
}

void IrcClient::send_invitation(const std::string& chan_name, const std::string& nick)
{
  this->send_message(IrcMessage("INVITE", {nick, chan_name}));
}

void IrcClient::send_topic_command(const std::string& chan_name, const std::string& topic)
{
  this->send_message(IrcMessage("TOPIC", {chan_name, topic}));
}

void IrcClient::send_quit_command(const std::string& reason)
{
  this->send_message(IrcMessage("QUIT", {reason}), {}, false);
}

void IrcClient::send_join_command(const std::string& chan_name, const std::string& password)
{
  if (!this->welcomed)
    {
      const auto it = std::find_if(begin(this->channels_to_join), end(this->channels_to_join),
                                   [&chan_name](const auto& pair) { return std::get<0>(pair) == chan_name; });
      if (it == end(this->channels_to_join))
        this->channels_to_join.emplace_back(chan_name, password);
    }
  else if (password.empty())
    this->send_message(IrcMessage("JOIN", {chan_name}));
  else
    this->send_message(IrcMessage("JOIN", {chan_name, password}));
  this->start();
}

bool IrcClient::send_channel_message(const std::string& chan_name, const std::string& body,
                                     MessageCallback callback)
{
  IrcChannel* channel = this->get_channel(chan_name);
  if (!channel->joined)
    {
      log_warning("Cannot send message to channel ", chan_name, ", it is not joined");
      return false;
    }
  // The max size is 512, taking into account the whole message, not just
  // the text we send.
  // This includes our own nick, constants for username and host (because these
  // are notoriously hard to know what the server will use), in addition to the basic
  // components of the message we send (command name, chan name, \r\n etc.)
  //  : + NICK + ! + USER + @ + HOST + <space> + PRIVMSG + <space> + CHAN + <space> + : + \r\n
  // 63 is the maximum hostname length defined by the protocol.  10 seems to be
  // the username limit.
  constexpr auto max_username_size = 10;
  constexpr auto max_hostname_size = 63;
  const auto line_size = 512 -
                         this->current_nick.size() - max_username_size - max_hostname_size -
                         ::strlen(":!@ PRIVMSG ") - chan_name.length() - ::strlen(" :\r\n");
  const auto lines = cut(body, line_size);
  for (const auto& line: lines)
    this->send_message(IrcMessage("PRIVMSG", {chan_name, line}), callback);
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

void IrcClient::on_pong(const IrcMessage&)
{
}

void IrcClient::send_ping_command()
{
  this->send_message(IrcMessage("PING", {"biboumi"}));
}

void IrcClient::forward_server_message(const IrcMessage& message)
{
  const std::string from = message.prefix;
  std::string body;
  for (auto it = std::next(message.arguments.begin()); it != message.arguments.end(); ++it)
    body += *it + ' ';

  this->bridge.send_xmpp_message(this->hostname, from, body);
}

void IrcClient::on_notice(const IrcMessage& message)
{
  std::string from = message.prefix;
  std::string to = message.arguments[0];
  const std::string body = message.arguments[1];

  // Handle notices starting with [#channame] as if they were sent to that channel
  if (body.size() > 3 && body[0] == '[')
    {
      const auto chan_prefix = body[1];
      auto end = body.find(']');
      if (this->chantypes.find(chan_prefix) != this->chantypes.end() && end != std::string::npos)
        to = body.substr(1, end - 1);
    }

  if (!body.empty() && body[0] == '\01' && body[body.size() - 1] == '\01')
    // Do not forward the notice to the user if it's a CTCP command
    return ;

  if (!to.empty() && this->chantypes.find(to[0]) == this->chantypes.end())
    {
      // The notice is for us precisely.

      // Find out if we already sent a private message to this user. If yes
      // we treat that message as a private message coming from
      // it. Otherwise we treat it as a notice coming from the server.
      IrcUser user(from);
      std::string nick = utils::tolower(user.nick);
      if (this->nicks_to_treat_as_private.find(nick) !=
          this->nicks_to_treat_as_private.end())
        { // We previously sent a message to that nick)
          this->bridge.send_message({nick, this->hostname, Iid::Type::User}, nick, body,
                                     false);
        }
      else
        this->bridge.send_xmpp_message(this->hostname, from, body);
    }
  else
    {
      // The notice was directed at a channel we are in. Modify the message
      // to indicate that it is a notice, and make it a MUC message coming
      // from the MUC JID
      IrcMessage modified_message(std::move(from), "PRIVMSG", {to, "\u000303[notice]\u0003 " + body});
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

void IrcClient::on_server_myinfo(const IrcMessage&)
{
}

void IrcClient::send_gateway_message(const std::string& message, const std::string& from)
{
  this->bridge.send_xmpp_message(this->hostname, from, message);
}

void IrcClient::set_and_forward_user_list(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[2]);
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->joined)
    {
      this->forward_server_message(message);
      return;
    }
  std::vector<std::string> nicks = utils::split(message.arguments[3], ' ');
  for (const std::string& nick: nicks)
    {
      // Just create this dummy user to parse and get its modes
      IrcUser tmp_user{nick, this->prefix_to_mode};
      // Does this concern ourself
      if (channel->get_self() && channel->find_user(tmp_user.nick) == channel->get_self())
        {
          // We now know our own modes, that’s all.
          channel->get_self()->modes = tmp_user.modes;
        }
      else
        { // Otherwise this is a new user
          const IrcUser *user = channel->add_user(nick, this->prefix_to_mode);
          this->bridge.send_user_join(this->hostname, chan_name, user, user->get_most_significant_mode(this->sorted_user_modes), false);
        }
    }
}

void IrcClient::on_channel_join(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[0]);
  IrcChannel* channel;
  channel = this->get_channel(chan_name);
  const std::string nick = message.prefix;
  IrcUser* user = channel->add_user(nick, this->prefix_to_mode);
  if (channel->joined == false)
    channel->set_self(user);
  else
    this->bridge.send_user_join(this->hostname, chan_name, user, user->get_most_significant_mode(this->sorted_user_modes), false);
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
      iid.type = Iid::Type::User;
      iid.set_local(nick);
      muc = false;
    }
  else
    iid.type = Iid::Type::Channel;
  if (!body.empty() && body[0] == '\01')
    {
      if (body.substr(1, 6) == "ACTION")
        this->bridge.send_message(iid, nick,
                  "/me" + body.substr(7, body.size() - 8), muc);
      else if (body.substr(1, 8) == "VERSION\01")
        this->bridge.send_iq_version_request(nick, this->hostname);
      else if (body.substr(1, 5) == "PING ")
        this->bridge.send_xmpp_ping_request(utils::tolower(nick), this->hostname,
                                             body.substr(6, body.size() - 7));
    }
  else
    this->bridge.send_message(iid, nick, body, muc);
}

void IrcClient::on_rpl_liststart(const IrcMessage&)
{
}

void IrcClient::on_rpl_list(const IrcMessage&)
{
}

void IrcClient::on_rpl_listend(const IrcMessage&)
{
}

void IrcClient::empty_motd(const IrcMessage&)
{
  this->motd.erase();
}

void IrcClient::on_invited(const IrcMessage& message)
{
  const std::string& chan_name = message.arguments[2];
  const std::string& invited_nick = message.arguments[1];

  this->bridge.send_xmpp_message(this->hostname, "", invited_nick + " has been invited to " + chan_name);
}

void IrcClient::on_empty_topic(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  log_debug("empty topic for ", chan_name);
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel)
    channel->topic.clear();
}

void IrcClient::on_motd_line(const IrcMessage& message)
{
  const std::string body = message.arguments[1];
  // We could send the MOTD without a line break between each IRC-message,
  // but sometimes it contains some ASCII art, we use line breaks to keep
  // them intact.
  this->motd += body+"\n";
}

void IrcClient::send_motd(const IrcMessage&)
{
  this->bridge.send_xmpp_message(this->hostname, "", this->motd);
}

void IrcClient::on_topic_received(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[message.arguments.size() - 2]);
  IrcUser author(message.prefix);
  IrcChannel* channel = this->get_channel(chan_name);
  channel->topic = message.arguments[message.arguments.size() - 1];
  channel->topic_author = author.nick;
  if (channel->joined)
    this->bridge.send_topic(this->hostname, chan_name, channel->topic, channel->topic_author);
}

void IrcClient::on_topic_who_time_received(const IrcMessage& message)
{
  IrcUser author(message.arguments[2]);
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  channel->topic_author = author.nick;
}

void IrcClient::on_channel_completely_joined(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  if (chan_name == "*" || channel->joined)
    {
      this->forward_server_message(message);
      return;
    }
  if (!channel->get_self())
    {
      log_error("End of NAMES list but we never received our own nick.");
      return;
    }
  channel->joined = true;
  this->bridge.send_user_join(this->hostname, chan_name, channel->get_self(),
                              channel->get_self()->get_most_significant_mode(this->sorted_user_modes), true);
  this->bridge.send_room_history(this->hostname, chan_name, this->history_limit);
  this->bridge.send_topic(this->hostname, chan_name, channel->topic, channel->topic_author);
}

void IrcClient::on_banlist(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->joined)
    {
      Iid iid;
      iid.set_local(chan_name);
      iid.set_server(this->hostname);
      iid.type = Iid::Type::Channel;
      std::string body{message.arguments[2] + " banned"};
      if (message.arguments.size() >= 4)
        {
          IrcUser by(message.arguments[3], this->prefix_to_mode);
          body += " by " + by.nick;
        }
      if (message.arguments.size() >= 5)
        body += " on " + message.arguments[4];

      this->bridge.send_message(iid, "", body, true);
    }
}

void IrcClient::on_banlist_end(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[1]);
  IrcChannel* channel = this->get_channel(chan_name);
  if (channel->joined)
    {
      Iid iid;
      iid.set_local(chan_name);
      iid.set_server(this->hostname);
      iid.type = Iid::Type::Channel;
      this->bridge.send_message(iid, "", message.arguments[2], true);
    }
}

void IrcClient::on_own_host_received(const IrcMessage& message)
{
  this->own_host = message.arguments[1];
  const std::string from = message.prefix;
  if (message.arguments.size() >= 3)
    this->bridge.send_xmpp_message(this->hostname, from,
                                   this->own_host + " " + message.arguments[2]);
  else
    this->bridge.send_xmpp_message(this->hostname, from, this->own_host +
                                                         " is now your displayed host");
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
  for (const auto& pair: this->channels)
  {
    Iid iid;
    iid.set_local(pair.first);
    iid.set_server(this->hostname);
    iid.type = Iid::Type::Channel;
    this->bridge.send_nickname_conflict_error(iid, nickname);
  }
}

void IrcClient::on_nickname_change_too_fast(const IrcMessage& message)
{
  const std::string nickname = message.arguments[1];
  std::string txt;
  if (message.arguments.size() >= 3)
    txt = message.arguments[2];
  this->on_generic_error(message);
  for (const auto& pair: this->channels)
  {
    Iid iid;
    iid.set_local(pair.first);
    iid.set_server(this->hostname);
    iid.type = Iid::Type::Channel;
    this->bridge.send_presence_error(iid, nickname,
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

void IrcClient::on_useronchannel(const IrcMessage& message)
{
  this->send_gateway_message(message.arguments[1] + " " + message.arguments[3] + " "
                             + message.arguments[2]);
}

void IrcClient::on_welcome_message(const IrcMessage& message)
{
  this->current_nick = message.arguments[0];
  this->welcomed = true;
#ifdef USE_DATABASE
  auto options = Database::get_irc_server_options(this->bridge.get_bare_jid(),
                                                  this->get_hostname());
  const auto commands = Database::get_after_connection_commands(options);
  for (const auto& command: commands)
    this->send_raw(command.col<Database::AfterConnectionCommand>());
#endif
  // Install a repeated events to regularly send a PING
  TimedEventsManager::instance().add_event(TimedEvent(240s, std::bind(&IrcClient::send_ping_command, this),
                                                      "PING" + this->hostname + this->bridge.get_jid()));
  std::string channels{};
  std::string channels_with_key{};
  std::string keys{};

  for (const auto& tuple: this->channels_to_join)
    {
      const auto& chan = std::get<0>(tuple);
      const auto& key = std::get<1>(tuple);
      if (chan.empty())
        continue;
      if (!key.empty())
        {
          if (keys.size() + channels_with_key.size() >= 300)
            { // Arbitrary size, to make sure we never send more than 512
              this->send_join_command(channels_with_key, keys);
              channels_with_key.clear();
              keys.clear();
            }
          if (!keys.empty())
            keys += ",";
          keys += key;
          if (!channels_with_key.empty())
            channels_with_key += ",";
          channels_with_key += chan;
        }
      else
        {
          if (channels.size() >= 300)
            { // Arbitrary size, to make sure we never send more than 512
              this->send_join_command(channels, {});
              channels.clear();
            }
          if (!channels.empty())
            channels += ",";
          channels += chan;
        }
    }
  if (!channels.empty())
    this->send_join_command(channels, {});
  if (!channels_with_key.empty())
    this->send_join_command(channels_with_key, keys);
  this->channels_to_join.clear();
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
      bool self = channel->get_self() && channel->get_self()->nick == nick;
      auto user_ptr = channel->remove_user(user);
      if (self)
      {
        this->channels.erase(utils::tolower(chan_name));
        // channel pointer is now invalid
        channel = nullptr;
      }
      Iid iid;
      iid.set_local(chan_name);
      iid.set_server(this->hostname);
      iid.type = Iid::Type::Channel;
      this->bridge.send_muc_leave(iid, *user_ptr, txt, self, true, {}, this);
    }
}

void IrcClient::on_error(const IrcMessage& message)
{
  const std::string leave_message = message.arguments[0];
  // The user is out of all the channels
  for (const auto& pair: this->channels)
  {
    Iid iid;
    iid.set_local(pair.first);
    iid.set_server(this->hostname);
    iid.type = Iid::Type::Channel;
    IrcChannel* channel = pair.second.get();
    if (!channel->joined)
      continue;
    this->bridge.send_muc_leave(iid, *channel->get_self(), leave_message, true, false, {}, this);
  }
  this->channels.clear();
  this->send_gateway_message("ERROR: " + leave_message);
}

void IrcClient::on_quit(const IrcMessage& message)
{
  std::string txt;
  if (message.arguments.size() >= 1)
    txt = message.arguments[0];
  for (const auto& pair: this->channels)
    {
      const std::string& chan_name = pair.first;
      IrcChannel* channel = pair.second.get();
      const IrcUser* user = channel->find_user(message.prefix);
      if (!user)
        continue;
      bool self = false;
      if (user == channel->get_self())
        self = true;
      Iid iid;
      iid.set_local(chan_name);
      iid.set_server(this->hostname);
      iid.type = Iid::Type::Channel;
      this->bridge.send_muc_leave(iid, *user, txt, self, false, {}, this);
      channel->remove_user(user);
    }
}

void IrcClient::on_nick(const IrcMessage& message)
{
  const std::string new_nick = IrcUser(message.arguments[0]).nick;
  const std::string current_nick = IrcUser(message.prefix).nick;
  const auto change_nick_func = [this, &new_nick, &current_nick](const std::string& chan_name, const IrcChannel* channel)
  {
    IrcUser* user;
    if (channel->get_self() && channel->get_self()->nick == current_nick)
      user = channel->get_self();
    else
      user = channel->find_user(current_nick);
    if (user)
      {
        std::string old_nick = user->nick;
        Iid iid(chan_name, this->hostname, Iid::Type::Channel);
        const bool self = channel->get_self()->nick == old_nick;
        const char user_mode = user->get_most_significant_mode(this->sorted_user_modes);
        this->bridge.send_nick_change(std::move(iid), old_nick, new_nick, user_mode, self);
        user->nick = new_nick;
        if (self)
          {
            channel->get_self()->nick = new_nick;
            this->current_nick = new_nick;
          }
      }
  };

  for (const auto& pair: this->channels)
    {
      change_nick_func(pair.first, pair.second.get());
    }
}

void IrcClient::on_kick(const IrcMessage& message)
{
  const std::string chan_name = utils::tolower(message.arguments[0]);
  const std::string target_nick = message.arguments[1];
  const std::string reason = message.arguments[2];
  IrcChannel* channel = this->get_channel(chan_name);
  if (!channel->joined)
    return ;
  const IrcUser* target = channel->find_user(target_nick);
  if (!target)
    {
      log_warning("Received a KICK command from a nick absent from the channel.");
      return;
    }
  const bool self = channel->get_self() == target;
  if (self)
    channel->joined = false;
  IrcUser author(message.prefix);
  Iid iid;
  iid.set_local(chan_name);
  iid.set_server(this->hostname);
  iid.type = Iid::Type::Channel;
  this->bridge.kick_muc_user(std::move(iid), target_nick, reason, author.nick, self);
  channel->remove_user(target);
}

void IrcClient::on_invite(const IrcMessage& message)
{
  IrcUser author(message.prefix);
  Iid iid;
  iid.set_local(message.arguments[1]);
  iid.set_server(this->hostname);
  iid.type = Iid::Type::Channel;

  this->bridge.send_xmpp_invitation(iid, author.nick);
}

void IrcClient::on_mode(const IrcMessage& message)
{
  const std::string target = message.arguments[0];
  if (this->chantypes.find(target[0]) != this->chantypes.end())
    this->on_channel_mode(message);
  else
    this->on_user_mode(message);
}

void IrcClient::on_channel_bad_key(const IrcMessage& message)
{
  this->on_generic_error(message);
  const std::string& nickname = message.arguments[0];
  const std::string& channel = message.arguments[1];
  std::string text;
  if (message.arguments.size() > 2)
    text = message.arguments[2];

  this->bridge.send_presence_error({channel, this->hostname, Iid::Type::Channel}, nickname, "auth", "not-authorized", "", text);
}

void IrcClient::on_channel_mode(const IrcMessage& message)
{
  Iid iid;
  iid.set_local(message.arguments[0]);
  iid.set_server(this->hostname);
  iid.type = Iid::Type::Channel;
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
  this->bridge.send_message(iid, "", "Mode " + iid.get_local() +
                                      " [" + mode_arguments + "] by " + user.nick,
                             true, this->is_channel_joined(iid.get_local()));
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
              for (const auto& pair: this->prefix_to_mode)
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
                  log_warning("Trying to set mode for non-existing user '", target
                             , "' in channel", iid.get_local());
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
      this->bridge.send_affiliation_role_change(iid, u->nick, most_significant_mode);
    }
}

void IrcClient::set_throttle_limit(long int limit)
{
  this->tokens_bucket.set_limit(limit);
}

void IrcClient::on_user_mode(const IrcMessage& message)
{
  this->bridge.send_xmpp_message(this->hostname, "",
                                  "User mode for " + message.arguments[0] +
                                  " is [" + message.arguments[1] + "]");
}

void IrcClient::on_unknown_message(const IrcMessage& message)
{
  if (message.arguments.size() < 2)
    return ;
  std::string from = message.prefix;
  std::stringstream ss;
  for (auto it = message.arguments.begin() + 1; it != message.arguments.end(); ++it)
    {
      ss << *it;
      if (it + 1 != message.arguments.end())
        ss << " ";
    }
  this->bridge.send_xmpp_message(this->hostname, from, ss.str());
}

size_t IrcClient::number_of_joined_channels() const
{
  return this->channels.size();
}

#ifdef BOTAN_FOUND
bool IrcClient::abort_on_invalid_cert() const
{
#ifdef USE_DATABASE
  auto options = Database::get_irc_server_options(this->bridge.get_bare_jid(), this->hostname);
  return options.col<Database::VerifyCert>();
#endif
  return true;
}
#endif

long int IrcClient::get_throttle_limit() const
{
#ifdef USE_DATABASE
  return Database::get_irc_server_options(this->bridge.get_bare_jid(), this->hostname).col<Database::ThrottleLimit>();
#else
  return 10;
#endif
}
