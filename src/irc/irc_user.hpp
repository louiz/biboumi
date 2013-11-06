#ifndef IRC_USER_INCLUDED
# define IRC_USER_INCLUDED

#include <string>

/**
 * Keeps various information about one IRC channel user
 */
class IrcUser
{
public:
  explicit IrcUser(const std::string& name);

  std::string nick;
  std::string host;

private:
  IrcUser(const IrcUser&) = delete;
  IrcUser(IrcUser&&) = delete;
  IrcUser& operator=(const IrcUser&) = delete;
  IrcUser& operator=(IrcUser&&) = delete;
};

#endif // IRC_USER_INCLUDED
