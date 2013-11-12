#include <xmpp/xmpp_component.hpp>
#include <network/poller.hpp>
#include <config/config.hpp>

#include <iostream>
#include <memory>

/**
 * Provide an helpful message to help the user write a minimal working
 * configuration file.
 */
int config_help(const std::string& missing_option)
{
  if (!missing_option.empty())
    std::cerr << "Error: empty value for option " << missing_option << "." << std::endl;
  std::cerr <<
    "Please provide a configuration file filled like this:\n\n"
    "hostname=irc.example.com\npassword=S3CR3T"
            << std::endl;
  return 1;
}

int main(int ac, char** av)
{
  if (ac > 1)
    Config::filename = av[1];
  Config::file_must_exist = true;
  std::cerr << "Using configuration file: " << Config::filename << std::endl;

  std::string password;
  try { // The file must exist
    password = Config::get("password", "");
  }
  catch (const std::ios::failure& e) {
    return config_help("");
  }
  const std::string hostname = Config::get("hostname", "");
  if (password.empty())
    return config_help("hostname");
  if (hostname.empty())
    return config_help("password");
  std::shared_ptr<XmppComponent> xmpp_component =
    std::make_shared<XmppComponent>(hostname, password);

  Poller p;
  p.add_socket_handler(xmpp_component);
  xmpp_component->start();
  while (p.poll())
    ;
  return 0;
}
