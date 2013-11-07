#include <bridge/bridge.hpp>
#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>

#include <iostream>

Bridge::Bridge(const std::string& user_jid, XmppComponent* xmpp, Poller* poller):
  user_jid(user_jid),
  xmpp(xmpp),
  poller(poller)
{
}

Bridge::~Bridge()
{
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
  irc->send_channel_message(iid.chan, body);
  this->xmpp->send_muc_message(iid.chan + "%" + iid.server, irc->get_own_nick(), body, this->user_jid);
}

void Bridge::send_muc_message(const Iid& iid, const std::string& nick, const std::string& body)
{
  this->xmpp->send_muc_message(iid.chan + "%" + iid.server, nick, body, this->user_jid);
}

void Bridge::send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg)
{
  std::string body;
  if (!author.empty())
    body = std::string("[") + author + std::string("] ") + msg;
  else
    body = msg;
  this->xmpp->send_message(from, body, this->user_jid);
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
  this->xmpp->send_topic(chan_name + "%" + hostname, topic, this->user_jid);
}
