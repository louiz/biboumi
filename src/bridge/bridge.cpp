#include <bridge/bridge.hpp>
#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>

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

void Bridge::join_irc_channel(const Iid& iid, const std::string& username)
{
  IrcClient* irc = this->get_irc_client(iid.server, username);
  irc->send_join_command(iid.chan);
}

void Bridge::send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg)
{
  const std::string body = std::string("[") + author + std::string("] ") + msg;
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
