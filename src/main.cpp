#include <network/poller.hpp>
#include <xmpp/xmpp_component.hpp>

#include <memory>

int main()
{
  Poller p;
  std::shared_ptr<XmppComponent> xmpp_component =
    std::make_shared<XmppComponent>("irc.localhost", "secret");
  p.add_socket_handler(xmpp_component);
  xmpp_component->start();
  while (p.poll())
    ;
  return 0;
}
