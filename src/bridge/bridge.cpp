#include <bridge/bridge.hpp>
#include <bridge/colors.hpp>
#include <xmpp/xmpp_component.hpp>
#include <xmpp/xmpp_stanza.hpp>
#include <irc/irc_message.hpp>
#include <network/poller.hpp>
#include <utils/encoding.hpp>
#include <utils/tolower.hpp>
#include <logger/logger.hpp>
#include <utils/revstr.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <stdexcept>
#include <iostream>
#include <tuple>

using namespace std::string_literals;

static const char* action_prefix = "\01ACTION ";

Bridge::Bridge(const std::string& user_jid, XmppComponent* xmpp, std::shared_ptr<Poller> poller):
  user_jid(user_jid),
  xmpp(xmpp),
  poller(poller)
{
}

Bridge::~Bridge()
{
}

/**
 * Return the role and affiliation, corresponding to the given irc mode */
static std::tuple<std::string, std::string> get_role_affiliation_from_irc_mode(const char mode)
{
  if (mode == 'a' || mode == 'q')
    return std::make_tuple("moderator", "owner");
  else if (mode == 'o')
    return std::make_tuple("moderator", "admin");
  else if (mode == 'h')
    return std::make_tuple("moderator", "member");
  else if (mode == 'v')
    return std::make_tuple("participant", "member");
  else
    return std::make_tuple("participant", "none");
}

void Bridge::shutdown(const std::string& exit_message)
{
  for (auto it = this->irc_clients.begin(); it != this->irc_clients.end(); ++it)
  {
    it->second->send_quit_command(exit_message);
    it->second->leave_dummy_channel(exit_message);
  }
}

void Bridge::clean()
{
  auto it = this->irc_clients.begin();
  while (it != this->irc_clients.end())
  {
    IrcClient* client = it->second.get();
    if (!client->is_connected() && !client->is_connecting())
      it = this->irc_clients.erase(it);
    else
      ++it;
  }
}

const std::string& Bridge::get_jid() const
{
  return this->user_jid;
}

Xmpp::body Bridge::make_xmpp_body(const std::string& str)
{
  std::string res;
  if (utils::is_valid_utf8(str.c_str()))
    res = str;
  else
    res = utils::convert_to_utf8(str, "ISO-8859-1");
  return irc_format_to_xhtmlim(res);
}

IrcClient* Bridge::get_irc_client(const std::string& hostname, const std::string& username)
{
  try
    {
      return this->irc_clients.at(hostname).get();
    }
  catch (const std::out_of_range& exception)
    {
      this->irc_clients.emplace(hostname, std::make_shared<IrcClient>(this->poller, hostname, username, this));
      std::shared_ptr<IrcClient> irc = this->irc_clients.at(hostname);
      return irc.get();
    }
}

IrcClient* Bridge::get_irc_client(const std::string& hostname)
{
  try
    {
      return this->irc_clients.at(hostname).get();
    }
  catch (const std::out_of_range& exception)
    {
      return nullptr;
    }
}

bool Bridge::join_irc_channel(const Iid& iid, const std::string& username, const std::string& password)
{
  IrcClient* irc = this->get_irc_client(iid.get_server(), username);
  if (iid.get_local().empty())
    { // Join the dummy channel
      if (irc->is_welcomed())
        {
          if (irc->get_dummy_channel().joined)
            return false;
          // Immediately simulate a message coming from the IRC server saying that we
          // joined the channel
          const IrcMessage join_message(irc->get_nick(), "JOIN", {""});
          irc->on_channel_join(join_message);
          const IrcMessage end_join_message(std::string(iid.get_server()), "366",
                                            {irc->get_nick(),
                                                "", "End of NAMES list"});
          irc->on_channel_completely_joined(end_join_message);
        }
      else
        {
          irc->get_dummy_channel().joining = true;
          irc->start();
        }
      return true;
    }
  if (irc->is_channel_joined(iid.get_local()) == false)
    {
      irc->send_join_command(iid.get_local(), password);
      return true;
    }
  return false;
}

void Bridge::send_channel_message(const Iid& iid, const std::string& body)
{
  if (iid.get_local().empty() || iid.get_server().empty())
    {
      log_warning("Cannot send message to channel: [" << iid.get_local() << "] on server [" << iid.get_server() << "]");
      return;
    }
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (!irc)
    {
      log_warning("Cannot send message: no client exist for server " << iid.get_server());
      return;
    }
  // Because an IRC message cannot contain \n, we need to convert each line
  // of text into a separate IRC message. For conveniance, we also cut the
  // message into submessages on the XMPP side, this way the user of the
  // gateway sees what was actually sent over IRC.  For example if an user
  // sends “hello\n/me waves”, two messages will be generated: “hello” and
  // “/me waves”. Note that the “command” handling (messages starting with
  // /me, /mode, etc) is done for each message generated this way. In the
  // above example, the /me will be interpreted.
  std::vector<std::string> lines = utils::split(body, '\n', true);
  if (lines.empty())
    return ;
  for (const std::string& line: lines)
    {
      if (line.substr(0, 6) == "/mode ")
        {
          std::vector<std::string> args = utils::split(line.substr(6), ' ', false);
          irc->send_mode_command(iid.get_local(), args);
          continue;             // We do not want to send that back to the
                                // XMPP user, that’s not a textual message.
        }
      else if (line.substr(0, 4) == "/me ")
        irc->send_channel_message(iid.get_local(), action_prefix + line.substr(4) + "\01");
      else
        irc->send_channel_message(iid.get_local(), line);
      this->xmpp->send_muc_message(std::to_string(iid), irc->get_own_nick(),
                                   this->make_xmpp_body(line), this->user_jid);
    }
}

void Bridge::forward_affiliation_role_change(const Iid& iid, const std::string& nick,
                                             const std::string& affiliation,
                                             const std::string& role)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (!irc)
    return;
  IrcChannel* chan = irc->get_channel(iid.get_local());
  if (!chan || !chan->joined)
    return;
  IrcUser* user = chan->find_user(nick);
  if (!user)
    return;
  // For each affiliation or role, we have a “maximal” mode that we want to
  // set. We must remove any superior mode at the same time. For example if
  // the user already has +o mode, and we set its affiliation to member, we
  // remove the +o mode, and add +v.  For each “superior” mode (for example,
  // for +v, the superior modes are 'h', 'a', 'o' and 'q') we check if that
  // user has it, and if yes we remove that mode

  std::size_t nb = 1;               // the number of times the nick must be
                                    // repeated in the argument list
  std::string modes;                // The string of modes to
                                    // add/remove. For example "+v-aoh"
  std::vector<char> modes_to_remove; // List of modes to check for removal
  if (affiliation == "none")
    {
      modes = "";
      nb = 0;
      modes_to_remove = {'v', 'h', 'o', 'a', 'q'};
    }
  else if (affiliation == "member")
    {
      modes = "+v";
      modes_to_remove = {'h', 'o', 'a', 'q'};
    }
  else if (role == "moderator")
    {
      modes = "+h";
      modes_to_remove = {'o', 'a', 'q'};
    }
  else if (affiliation == "admin")
    {
      modes = "+o";
      modes_to_remove = {'a', 'q'};
    }
  else if (affiliation == "owner")
    {
      modes = "+a";
      modes_to_remove = {'q'};
    }
  else
    return;
  for (const char mode: modes_to_remove)
    if (user->modes.find(mode) != user->modes.end())
      {
        modes += "-"s + mode;
        nb++;
      }
  if (modes.empty())
    return;
  std::vector<std::string> args(nb, nick);
  args.insert(args.begin(), modes);
  irc->send_mode_command(iid.get_local(), args);
}

void Bridge::send_private_message(const Iid& iid, const std::string& body, const std::string& type)
{
  if (iid.get_local().empty() || iid.get_server().empty())
    return ;
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (!irc)
    {
      log_warning("Cannot send message: no client exist for server " << iid.get_server());
      return;
    }
  std::vector<std::string> lines = utils::split(body, '\n', true);
  if (lines.empty())
    return ;
  for (const std::string& line: lines)
    {
      if (line.substr(0, 4) == "/me ")
        irc->send_private_message(iid.get_local(), action_prefix + line.substr(4) + "\01", type);
      else
        irc->send_private_message(iid.get_local(), line, type);
    }
}

void Bridge::leave_irc_channel(Iid&& iid, std::string&& status_message)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (irc)
    irc->send_part_command(iid.get_local(), status_message);
}

void Bridge::send_irc_nick_change(const Iid& iid, const std::string& new_nick)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (irc)
    irc->send_nick_command(new_nick);
}

void Bridge::send_irc_kick(const Iid& iid, const std::string& target, const std::string& reason,
                           const std::string& iq_id, const std::string& to_jid)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());

  if (!irc)
    return;

  irc->send_kick_command(iid.get_local(), target, reason);
  irc_responder_callback_t cb = [this, target, iq_id, to_jid, iid](const std::string& irc_hostname,
                                                                   const IrcMessage& message) -> bool
    {
      if (irc_hostname != iid.get_server())
        return false;
      if (message.command == "KICK" && message.arguments.size() >= 2)
        {
          const std::string target_later = message.arguments[1];
          const std::string chan_name_later = utils::tolower(message.arguments[0]);
          if (target_later != target || chan_name_later != iid.get_local())
            return false;
          this->xmpp->send_iq_result(iq_id, to_jid, std::to_string(iid));
        }
      else if (message.command == "401" && message.arguments.size() >= 2)
        {
          const std::string target_later = message.arguments[1];
          if (target_later != target)
            return false;
          std::string error_message = "No such nick";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp->send_stanza_error("iq", to_jid, std::to_string(iid), iq_id, "cancel", "item-not-found",
                                        error_message, false);
        }
      else if (message.command == "482" && message.arguments.size() >= 2)
        {
          const std::string chan_name_later = utils::tolower(message.arguments[1]);
          if (chan_name_later != iid.get_local())
            return false;
          std::string error_message = "You're not channel operator";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp->send_stanza_error("iq", to_jid, std::to_string(iid), iq_id, "cancel", "not-allowed",
                                        error_message, false);
        }
      return true;
    };
  this->add_waiting_irc(std::move(cb));
}

void Bridge::set_channel_topic(const Iid& iid, const std::string& subject)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (irc)
    irc->send_topic_command(iid.get_local(), subject);
}

void Bridge::send_xmpp_version_to_irc(const Iid& iid, const std::string& name, const std::string& version, const std::string& os)
{
  std::string result(name + " " + version + " " + os);

  this->send_private_message(iid, "\01VERSION "s + result + "\01", "NOTICE");
}

void Bridge::send_irc_ping_result(const Iid& iid, const std::string& id)
{
  this->send_private_message(iid, "\01PING "s + utils::revstr(id), "NOTICE");
}

void Bridge::send_irc_user_ping_request(const std::string& irc_hostname, const std::string& nick,
                                        const std::string& iq_id, const std::string& to_jid,
                                        const std::string& from_jid)
{
  Iid iid(nick + "!" + irc_hostname);
  this->send_private_message(iid, "\01PING " + iq_id + "\01");

  irc_responder_callback_t cb = [this, nick, iq_id, to_jid, irc_hostname, from_jid](const std::string& hostname, const IrcMessage& message) -> bool
    {
      if (irc_hostname != hostname)
        return false;
      IrcUser user(message.prefix);
      const std::string body = message.arguments[1];
      if (message.command == "NOTICE" && user.nick == nick &&
          message.arguments.size() >= 2 && body.substr(0, 6) == "\01PING ")
        {
          const std::string id = body.substr(6, body.size() - 6);
          if (id != iq_id)
            return false;
          Jid jid(from_jid);
          this->xmpp->send_iq_result(iq_id, to_jid, jid.local);
          return true;
        }
      if (message.command == "401" && message.arguments.size() >= 2
          && message.arguments[1] == nick)
        {
          std::string error_message = "No such nick";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp->send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "service-unavailable",
                                        error_message, true);
          return true;
        }

      return false;
    };
  this->add_waiting_irc(std::move(cb));
}

void Bridge::send_irc_participant_ping_request(const Iid& iid, const std::string& nick,
                                               const std::string& iq_id, const std::string& to_jid,
                                               const std::string& from_jid)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (!irc)
    {
      this->xmpp->send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "item-not-found",
                                    "Not connected to IRC server"s + iid.get_server(), true);
      return;
    }
  IrcChannel* chan = irc->get_channel(iid.get_local());
  if (!chan->joined)
    {
      this->xmpp->send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "not-allowed",
                                    "", true);
      return;
    }
  if (chan->get_self()->nick != nick && !chan->find_user(nick))
    {
      this->xmpp->send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "item-not-found",
                                    "Recipient not in room", true);
      return;
    }

  // The user is in the room, send it a direct PING
  this->send_irc_user_ping_request(iid.get_server(), nick, iq_id, to_jid, from_jid);
}

void Bridge::on_gateway_ping(const std::string& irc_hostname, const std::string& iq_id, const std::string& to_jid,
                             const std::string& from_jid)
{
  Jid jid(from_jid);
  if (irc_hostname.empty() || this->get_irc_client(irc_hostname))
    this->xmpp->send_iq_result(iq_id, to_jid, jid.local);
  else
    this->xmpp->send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "service-unavailable",
                                  "", true);
}

void Bridge::send_irc_version_request(const std::string& irc_hostname, const std::string& target,
                                      const std::string& iq_id, const std::string& to_jid,
                                      const std::string& from_jid)
{
  Iid iid(target + "!" + irc_hostname);
  this->send_private_message(iid, "\01VERSION\01");
  // TODO, add a timer to remove that waiting iq if the server does not
  // respond with a matching command before n seconds
  irc_responder_callback_t cb = [this, target, iq_id, to_jid, irc_hostname, from_jid](const std::string& hostname, const IrcMessage& message) -> bool
    {
      if (irc_hostname != hostname)
        return false;
      IrcUser user(message.prefix);
      if (message.command == "NOTICE" && user.nick == target &&
          message.arguments.size() >= 2 && message.arguments[1].substr(0, 9) == "\01VERSION ")
        {
          // remove the "\01VERSION " and the "\01" parts from the string
          const std::string version = message.arguments[1].substr(9, message.arguments[1].size() - 10);
          this->xmpp->send_version(iq_id, to_jid, from_jid, version);
          return true;
        }
      if (message.command == "401" && message.arguments.size() >= 2
          && message.arguments[1] == target)
        {
          std::string error_message = "No such nick";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp->send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "item-not-found",
                                        error_message, true);
          return true;
        }
      return false;
    };
  this->add_waiting_irc(std::move(cb));
}

void Bridge::send_message(const Iid& iid, const std::string& nick, const std::string& body, const bool muc)
{
  if (muc)
    this->xmpp->send_muc_message(std::to_string(iid), nick,
                                 this->make_xmpp_body(body), this->user_jid);
  else
    {
      std::string target = std::to_string(iid);
      bool fulljid = false;
      auto it = this->preferred_user_from.find(iid.get_local());
      if (it != this->preferred_user_from.end())
        {
          target = it->second;
          fulljid = true;
        }
      this->xmpp->send_message(target, this->make_xmpp_body(body),
                               this->user_jid, "chat", fulljid);
    }
}

void Bridge::send_presence_error(const Iid& iid, const std::string& nick,
                                 const std::string& type, const std::string& condition,
                                 const std::string& error_code, const std::string& text)
{
  this->xmpp->send_presence_error(std::to_string(iid), nick, this->user_jid, type, condition, error_code, text);
}

void Bridge::send_muc_leave(Iid&& iid, std::string&& nick, const std::string& message, const bool self)
{
  this->xmpp->send_muc_leave(std::to_string(iid), std::move(nick), this->make_xmpp_body(message), this->user_jid, self);
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (irc && irc->number_of_joined_channels() == 0)
    irc->send_quit_command("");
}

void Bridge::send_nick_change(Iid&& iid,
                              const std::string& old_nick,
                              const std::string& new_nick,
                              const char user_mode,
                              const bool self)
{
  std::string affiliation;
  std::string role;
  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(user_mode);

  this->xmpp->send_nick_change(std::to_string(iid),
                               old_nick, new_nick, affiliation, role, this->user_jid, self);
}

void Bridge::send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg)
{
  std::string body;
  if (!author.empty())
    {
      IrcUser user(author);
      body = "\u000303"s + user.nick + (user.host.empty()?
                                        "\u0003: ":
                                        (" (\u000310" + user.host + "\u000303)\u0003: ")) + msg;
    }
  else
    body = msg;
  this->xmpp->send_message(from, this->make_xmpp_body(body), this->user_jid, "chat");
}

void Bridge::send_user_join(const std::string& hostname,
                            const std::string& chan_name,
                            const IrcUser* user,
                            const char user_mode,
                            const bool self)
{
  std::string affiliation;
  std::string role;
  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(user_mode);

  this->xmpp->send_user_join(chan_name + "%" + hostname, user->nick, user->host,
                             affiliation, role, this->user_jid, self);
}

void Bridge::send_topic(const std::string& hostname, const std::string& chan_name, const std::string& topic)
{
  this->xmpp->send_topic(chan_name + "%" + hostname, this->make_xmpp_body(topic), this->user_jid);
}

std::string Bridge::get_own_nick(const Iid& iid)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (irc)
    return irc->get_own_nick();
  return "";
}

size_t Bridge::active_clients() const
{
  return this->irc_clients.size();
}

void Bridge::kick_muc_user(Iid&& iid, const std::string& target, const std::string& reason, const std::string& author)
{
  this->xmpp->kick_user(std::to_string(iid), target, reason, author, this->user_jid);
}

void Bridge::send_nickname_conflict_error(const Iid& iid, const std::string& nickname)
{
  this->xmpp->send_presence_error(std::to_string(iid), nickname, this->user_jid, "cancel", "conflict", "409", "");
}

void Bridge::send_affiliation_role_change(const Iid& iid, const std::string& target, const char mode)
{
  std::string role;
  std::string affiliation;

  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(mode);
  this->xmpp->send_affiliation_role_change(std::to_string(iid), target, affiliation, role, this->user_jid);
}

void Bridge::send_iq_version_request(const std::string& nick, const std::string& hostname)
{
  this->xmpp->send_iq_version_request(nick + "!" + hostname, this->user_jid);
}

void Bridge::send_xmpp_ping_request(const std::string& nick, const std::string& hostname,
                                    const std::string& id)
{
  // Use revstr because the forwarded ping to target XMPP user must not be
  // the same that the request iq, but we also need to get it back easily
  // (revstr again)
  this->xmpp->send_ping_request(nick + "!" + hostname, this->user_jid, utils::revstr(id));
}

void Bridge::set_preferred_from_jid(const std::string& nick, const std::string& full_jid)
{
  auto it = this->preferred_user_from.find(nick);
  if (it == this->preferred_user_from.end())
    this->preferred_user_from.emplace(nick, full_jid);
  else
    this->preferred_user_from[nick] = full_jid;
}

void Bridge::remove_preferred_from_jid(const std::string& nick)
{
  auto it = this->preferred_user_from.find(nick);
  if (it != this->preferred_user_from.end())
    this->preferred_user_from.erase(it);
}

void Bridge::add_waiting_irc(irc_responder_callback_t&& callback)
{
  this->waiting_irc.emplace_back(std::move(callback));
}

void Bridge::trigger_on_irc_message(const std::string& irc_hostname, const IrcMessage& message)
{
  auto it = this->waiting_irc.begin();
  while (it != this->waiting_irc.end())
    {
      if ((*it)(irc_hostname, message) == true)
        it = this->waiting_irc.erase(it);
      else
        ++it;
    }
}
