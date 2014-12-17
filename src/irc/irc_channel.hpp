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
                    const std::map<char, char>& prefix_to_mode);
  IrcUser* find_user(const std::string& name) const;
  void remove_user(const IrcUser* user);
  void remove_all_users();

protected:
  std::unique_ptr<IrcUser> self;
  std::vector<std::unique_ptr<IrcUser>> users;

private:
  IrcChannel(const IrcChannel&) = delete;
  IrcChannel(IrcChannel&&) = delete;
  IrcChannel& operator=(const IrcChannel&) = delete;
  IrcChannel& operator=(IrcChannel&&) = delete;
};

/**
 * A special channel that is not actually linked to any real irc
 * channel. This is just a channel representing a connection to the
 * server. If an user wants to maintain the connection to the server without
 * having to be on any irc channel of that server, he can just join this
 * dummy channel.
 * It’s not actually dummy because it’s useful and it does things, but well.
 */
class DummyIrcChannel: public IrcChannel
{
public:
  explicit DummyIrcChannel();

  /**
   * This flag is at true whenever the user wants to join this channel, but
   * he is not yet connected to the server. When the connection is made, we
   * check that flag and if it’s true, we inform the user that he has just
   * joined that channel.
   * If the user is already connected to the server when he tries to join
   * the channel, we don’t use that flag, we just join it immediately.
   */
  bool joining;
private:
  DummyIrcChannel(const DummyIrcChannel&) = delete;
  DummyIrcChannel(DummyIrcChannel&&) = delete;
  DummyIrcChannel& operator=(const DummyIrcChannel&) = delete;
  DummyIrcChannel& operator=(DummyIrcChannel&&) = delete;
};

#endif // IRC_CHANNEL_INCLUDED
