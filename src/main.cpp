#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>
#include <config/config.hpp>

#include <iostream>
#include <memory>

int main(int ac, char** av)
{
  if (ac > 1)
    Config::filename = av[1];
  Config::file_must_exist = true;

  std::string password;
  try { // The file must exist
    password = Config::get("password", "");
  }
  catch (const std::ios::failure& e) {
    return 1;
  }
  if (password.empty())
    {
      std::cerr << "No password provided." << std::endl;
      return 1;
    }
  std::shared_ptr<XmppComponent> xmpp_component =
    std::make_shared<XmppComponent>("irc.abricot", password);

  Poller p;
  p.add_socket_handler(xmpp_component);
  xmpp_component->start();
  while (p.poll())
    ;
  return 0;
}
