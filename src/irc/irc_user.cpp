#include <irc/irc_user.hpp>

#include <iostream>

IrcUser::IrcUser(const std::string& name)
{
  const std::string::size_type sep = name.find("!");
  if (sep == std::string::npos)
    {
      if (name[0] == '@' || name[0] == '+')
        this->nick = name.substr(1);
      else
        this->nick = name;
    }
  else
    {
      if (name[0] == '@' || name[0] == '+')
        this->nick = name.substr(1, sep);
      else
        this->nick = name.substr(0, sep);
      this->host = name.substr(sep+1);
    }
  std::cout << "Created user: [" << this->nick << "!" << this->host << std::endl;
}
