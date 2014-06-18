#include <bridge/bridge.hpp>
#include <bridge/colors.hpp>
#include <xmpp/xmpp_component.hpp>
#include <irc/irc_message.hpp>
#include <network/poller.hpp>
#include <utils/encoding.hpp>
#include <logger/logger.hpp>
#include <utils/split.hpp>
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

bool Bridge::join_irc_channel(const Iid& iid, const std::string& username)
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
      irc->send_join_command(iid.get_local());
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

void Bridge::send_irc_kick(const Iid& iid, const std::string& target, const std::string& reason)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  if (irc)
    irc->send_kick_command(iid.get_local(), target, reason);
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

void Bridge::send_join_failed(const Iid& iid, const std::string& nick, const std::string& type, const std::string& condition, const std::string& text)
{
  this->xmpp->send_presence_error(std::to_string(iid), nick, this->user_jid, type, condition, text);
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

void Bridge::send_topic(const std::string& hostname, const std::string& chan_name, const std::string topic)
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
  this->xmpp->send_nickname_conflict_error(std::to_string(iid), nickname, this->user_jid);
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
