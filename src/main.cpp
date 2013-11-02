#include <libirc/irc_client.hpp>
#include <network/poller.hpp>

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
