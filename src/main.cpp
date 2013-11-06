#include <irc/irc_client.hpp>
#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>

#include <xmpp/xmpp_parser.hpp>
#include <xmpp/xmpp_stanza.hpp>

#include <memory>

#include <xmpp/jid.hpp>
#include <irc/iid.hpp>

#include <iostream>

int main()
{
  Poller p;
  // Now I'm the bridge, creating an ircclient because needed.
  std::shared_ptr<IrcClient> c = std::make_shared<IrcClient>();
  p.add_socket_handler(c);
  std::shared_ptr<IrcClient> d = std::make_shared<IrcClient>();
  p.add_socket_handler(d);
  std::shared_ptr<IrcClient> e = std::make_shared<IrcClient>();
  p.add_socket_handler(e);
  c->connect("localhost", "7877");
  d->connect("localhost", "7878");
  e->connect("localhost", "7879");
  while (true)
    p.poll();
  return 0;
}
