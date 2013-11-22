#include <bridge/bridge.hpp>
#include <bridge/colors.hpp>
#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>
#include <utils/encoding.hpp>

#include <utils/split.hpp>
#include <iostream>

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
      irc->start();
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
      std::cout << "Cannot send message to channel: [" << iid.chan << "] on server [" << iid.server << "]" << std::endl;
      return;
    }
  IrcClient* irc = this->get_irc_client(iid.server);
  if (!irc)
    {
      std::cout << "Cannot send message: no client exist for server " << iid.server << std::endl;
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

void Bridge::send_message(const Iid& iid, const std::string& nick, const std::string& body, const bool muc)
{
  if (muc)
    this->xmpp->send_muc_message(iid.chan + "%" + iid.server, nick,
                                 this->make_xmpp_body(body), this->user_jid);
  else
    this->xmpp->send_message(iid.chan + "%" + iid.server,
                             this->make_xmpp_body(body), this->user_jid);
}

void Bridge::send_muc_leave(Iid&& iid, std::string&& nick, const std::string& message, const bool self)
{
  this->xmpp->send_muc_leave(std::move(iid.chan) + "%" + std::move(iid.server), std::move(nick), this->make_xmpp_body(message), this->user_jid, self);
}

void Bridge::send_nick_change(Iid&& iid, const std::string& old_nick, const std::string& new_nick, const bool self)
{
  this->xmpp->send_nick_change(std::move(iid.chan) + "%" + std::move(iid.server),
                               old_nick, new_nick, this->user_jid, self);
}

void Bridge::send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg)
{
  std::string body;
  if (!author.empty())
    body = std::string("[") + author + std::string("] ") + msg;
  else
    body = msg;
  this->xmpp->send_message(from, this->make_xmpp_body(body), this->user_jid);
}

void Bridge::send_user_join(const std::string& hostname, const std::string& chan_name, const std::string nick)
{
  this->xmpp->send_user_join(chan_name + "%" + hostname, nick, this->user_jid);
}

void Bridge::send_self_join(const std::string& hostname, const std::string& chan_name, const std::string nick)
{
  this->xmpp->send_self_join(chan_name + "%" + hostname, nick, this->user_jid);
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

void Bridge::kick_muc_user(Iid&& iid, const std::string& target, const std::string& reason, const std::string& author)
{
  this->xmpp->kick_user(iid.chan + "%" + iid.server, target, reason, author, this->user_jid);
}
