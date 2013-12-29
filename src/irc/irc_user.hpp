#ifndef IRC_USER_INCLUDED
# define IRC_USER_INCLUDED

#include <string>
#include <map>
#include <set>

/**
 * Keeps various information about one IRC channel user
 */
class IrcUser
{
public:
  explicit IrcUser(const std::string& name,
                   const std::map<char, char>& prefix_to_mode);
  explicit IrcUser(const std::string& name);
  std::string nick;
  std::string host;
  std::set<char> modes;

private:
  IrcUser(const IrcUser&) = delete;
  IrcUser(IrcUser&&) = delete;
  IrcUser& operator=(const IrcUser&) = delete;
  IrcUser& operator=(IrcUser&&) = delete;
};

#endif // IRC_USER_INCLUDED
