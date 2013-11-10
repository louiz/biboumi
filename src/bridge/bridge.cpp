#include <bridge/bridge.hpp>
#include <bridge/colors.hpp>
#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>
#include <utils/encoding.hpp>

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

std::string Bridge::sanitize_for_xmpp(const std::string& str)
{
  std::string res;
  if (utils::is_valid_utf8(str.c_str()))
    res = str;
  else
    res = utils::convert_to_utf8(str, "ISO-8859-1");
  remove_irc_colors(res);
  return res;
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

void Bridge::join_irc_channel(const Iid& iid, const std::string& username)
{
  IrcClient* irc = this->get_irc_client(iid.server, username);
  irc->send_join_command(iid.chan);
}

void Bridge::send_channel_message(const Iid& iid, const std::string& body)
{
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
  if (body.substr(0, 4) == "/me ")
    irc->send_channel_message(iid.chan, action_prefix + body.substr(4) + "\01");
  else
    irc->send_channel_message(iid.chan, body);
  // We do not need to convert body to utf-8: it comes from our XMPP server,
  // so it's ok to send it back
  this->xmpp->send_muc_message(iid.chan + "%" + iid.server, irc->get_own_nick(), body, this->user_jid);
}

void Bridge::leave_irc_channel(Iid&& iid, std::string&& status_message)
{
  IrcClient* irc = this->get_irc_client(iid.server);
  if (irc)
    irc->send_part_command(iid.chan, status_message);
}

void Bridge::send_muc_message(const Iid& iid, const std::string& nick, const std::string& body)
{
  const std::string& utf8_body = this->sanitize_for_xmpp(body);
  if (utf8_body.substr(0, action_prefix_len) == action_prefix)
    { // Special case for ACTION (/me) messages:
      // "\01ACTION goes out\01" == "/me goes out"
      this->xmpp->send_muc_message(iid.chan + "%" + iid.server, nick,
        std::string("/me ") + utf8_body.substr(action_prefix_len, utf8_body.size() - action_prefix_len - 1),
        this->user_jid);
    }
  else
    this->xmpp->send_muc_message(iid.chan + "%" + iid.server, nick, utf8_body, this->user_jid);
}

void Bridge::send_muc_leave(Iid&& iid, std::string&& nick, std::string&& message, const bool self)
{
  this->xmpp->send_muc_leave(std::move(iid.chan) + "%" + std::move(iid.server), std::move(nick), this->sanitize_for_xmpp(message), this->user_jid, self);
}

void Bridge::send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg)
{
  std::string body;
  if (!author.empty())
    body = std::string("[") + author + std::string("] ") + msg;
  else
    body = msg;
  this->xmpp->send_message(from, this->sanitize_for_xmpp(body), this->user_jid);
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
  this->xmpp->send_topic(chan_name + "%" + hostname, this->sanitize_for_xmpp(topic), this->user_jid);
}
