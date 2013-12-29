#ifndef IRC_CHANNEL_INCLUDED
# define IRC_CHANNEL_INCLUDED

#include <irc/irc_user.hpp>
#include <memory>
#include <string>
#include <vector>
#include <map>

/**
 * Keep the state of a joined channel (the list of occupants with their
 * informations (mode, etc), the modes, etc)
 */
class IrcChannel
{
public:
  explicit IrcChannel();

  bool joined;
  std::string topic;
  void set_self(const std::string& name);
  IrcUser* get_self() const;
  IrcUser* add_user(const std::string& name,
                    const std::map<char, char> prefix_to_mode);
  IrcUser* find_user(const std::string& name);
  void remove_user(const IrcUser* user);

private:
  std::unique_ptr<IrcUser> self;
  std::vector<std::unique_ptr<IrcUser>> users;
  IrcChannel(const IrcChannel&) = delete;
  IrcChannel(IrcChannel&&) = delete;
  IrcChannel& operator=(const IrcChannel&) = delete;
  IrcChannel& operator=(IrcChannel&&) = delete;
};

#endif // IRC_CHANNEL_INCLUDED
