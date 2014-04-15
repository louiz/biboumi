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

static const char* action_prefix = "\01ACTION ";
static const size_t action_prefix_len = 8;

Bridge::Bridge(const std::string& user_jid, XmppComponent* xmpp, Poller* poller):
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

void Bridge::shutdown()
{
  for (auto it = this->irc_clients.begin(); it != this->irc_clients.end(); ++it)
  {
    const std::string exit_message("Gateway shutdown");
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
      this->irc_clients.emplace(hostname, std::make_shared<IrcClient>(hostname, username, this));
      std::shared_ptr<IrcClient> irc = this->irc_clients.at(hostname);
      this->poller->add_socket_handler(irc);
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
  IrcClient* irc = this->get_irc_client(iid.server, username);
  if (iid.chan.empty())
    { // Join the dummy channel
      if (irc->is_welcomed())
        {
          if (irc->get_dummy_channel().joined)
            return false;
          // Immediately simulate a message coming from the IRC server saying that we
          // joined the channel
          const IrcMessage join_message(irc->get_nick(), "JOIN", {""});
          irc->on_channel_join(join_message);
          const IrcMessage end_join_message(std::string(iid.server), "366",
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
  if (irc->is_channel_joined(iid.chan) == false)
    {
      irc->send_join_command(iid.chan);
      return true;
    }
  return false;
}

void Bridge::send_channel_message(const Iid& iid, const std::string& body)
{
  std::vector<std::string> lines = utils::split(body, '\n', true);
  if (lines.empty())
    return ;
  const std::string first_line = lines[0];
  if (iid.chan.empty() || iid.server.empty())
    {
      log_warning("Cannot send message to channel: [" << iid.chan << "] on server [" << iid.server << "]");
      return;
    }
  IrcClient* irc = this->get_irc_client(iid.server);
  if (!irc)
    {
      log_warning("Cannot send message: no client exist for server " << iid.server);
      return;
    }
  if (first_line.substr(0, 6) == "/mode ")
    {
      std::vector<std::string> args = utils::split(first_line.substr(6), ' ', false);
      irc->send_mode_command(iid.chan, args);
      return;
    }
  if (first_line.substr(0, 4) == "/me ")
    irc->send_channel_message(iid.chan, action_prefix + first_line.substr(4) + "\01");
  else
    irc->send_channel_message(iid.chan, first_line);
  // Send each of the other lines of the message as a separate IRC message
  for (std::vector<std::string>::const_iterator it = lines.begin() + 1; it != lines.end(); ++it)
    irc->send_channel_message(iid.chan, *it);
  // We do not need to convert body to utf-8: it comes from our XMPP server,
  // so it's ok to send it back
  this->xmpp->send_muc_message(iid.chan + "%" + iid.server, irc->get_own_nick(),
                               this->make_xmpp_body(body), this->user_jid);
}

void Bridge::send_private_message(const Iid& iid, const std::string& body)
{
  if (iid.chan.empty() || iid.server.empty())
    return ;
  IrcClient* irc = this->get_irc_client(iid.server);
  if (irc)
    irc->send_private_message(iid.chan, body);
}

void Bridge::leave_irc_channel(Iid&& iid, std::string&& status_message)
{
  IrcClient* irc = this->get_irc_client(iid.server);
  if (irc)
    irc->send_part_command(iid.chan, status_message);
}

void Bridge::send_irc_nick_change(const Iid& iid, const std::string& new_nick)
{
  IrcClient* irc = this->get_irc_client(iid.server);
  if (irc)
    irc->send_nick_command(new_nick);
}

void Bridge::send_irc_kick(const Iid& iid, const std::string& target, const std::string& reason)
{
  IrcClient* irc = this->get_irc_client(iid.server);
  if (irc)
    irc->send_kick_command(iid.chan, target, reason);
}

void Bridge::set_channel_topic(const Iid& iid, const std::string& subject)
{
  IrcClient* irc = this->get_irc_client(iid.server);
  if (irc)
    irc->send_topic_command(iid.chan, subject);
}

void Bridge::send_message(const Iid& iid, const std::string& nick, const std::string& body, const bool muc)
{
  if (muc)
    this->xmpp->send_muc_message(iid.chan + "%" + iid.server, nick,
                                 this->make_xmpp_body(body), this->user_jid);
  else
    this->xmpp->send_message(iid.chan + "%" + iid.server,
                             this->make_xmpp_body(body), this->user_jid, "chat");
}

void Bridge::send_muc_leave(Iid&& iid, std::string&& nick, const std::string& message, const bool self)
{
  this->xmpp->send_muc_leave(std::move(iid.chan) + "%" + std::move(iid.server), std::move(nick), this->make_xmpp_body(message), this->user_jid, self);
  IrcClient* irc = this->get_irc_client(iid.server);
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

  this->xmpp->send_nick_change(std::move(iid.chan) + "%" + std::move(iid.server),
                               old_nick, new_nick, affiliation, role, this->user_jid, self);
}

void Bridge::send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg)
{
  std::string body;
  if (!author.empty())
    body = std::string("[") + author + std::string("] ") + msg;
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
  IrcClient* irc = this->get_irc_client(iid.server);
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
  this->xmpp->kick_user(iid.chan + "%" + iid.server, target, reason, author, this->user_jid);
}

void Bridge::send_nickname_conflict_error(const Iid& iid, const std::string& nickname)
{
  this->xmpp->send_nickname_conflict_error(iid.chan + "%" + iid.server, nickname, this->user_jid);
}

void Bridge::send_affiliation_role_change(const Iid& iid, const std::string& target, const char mode)
{
  std::string role;
  std::string affiliation;

  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(mode);
  this->xmpp->send_affiliation_role_change(iid.chan + "%" + iid.server, target, affiliation, role, this->user_jid);
}
