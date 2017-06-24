#include <irc/irc_user.hpp>

#include <iostream>

IrcUser::IrcUser(const std::string& name,
                 const std::map<char, char>& prefix_to_mode)
{
  if (name.empty())
    return ;

  // One or more prefix (with multi-prefix support) may come before the
  // actual nick
  std::string::size_type name_begin = 0;
  while (name_begin != name.size())
    {
      const auto prefix = prefix_to_mode.find(name[name_begin]);
      // This is not a prefix
      if (prefix == prefix_to_mode.end())
        break;
      this->modes.insert(prefix->second);
      name_begin++;
    }

  const std::string::size_type sep = name.find('!', name_begin);
  if (sep == std::string::npos)
    this->nick = name.substr(name_begin);
  else
    {
      this->nick = name.substr(name_begin, sep-name_begin);
      this->host = name.substr(sep+1);
    }
}

IrcUser::IrcUser(const std::string& name):
  IrcUser(name, {})
{
}

void IrcUser::add_mode(const char mode)
{
  this->modes.insert(mode);
}

void IrcUser::remove_mode(const char mode)
{
  this->modes.erase(mode);
}

char IrcUser::get_most_significant_mode(const std::vector<char>& modes) const
{
  for (const char mode: modes)
    {
      if (this->modes.find(mode) != this->modes.end())
        return mode;
    }
  return 0;
}
