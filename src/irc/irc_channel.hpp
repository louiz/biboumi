#pragma once


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
  IrcChannel() = default;

  IrcChannel(const IrcChannel&) = delete;
  IrcChannel(IrcChannel&&) = delete;
  IrcChannel& operator=(const IrcChannel&) = delete;
  IrcChannel& operator=(IrcChannel&&) = delete;

  bool joined{false};
  // Set to true if we sent a PART but didn’t yet receive the PART ack from
  // the server
  bool parting{false};
  std::string topic{};
  std::string topic_author{};
  void set_self(IrcUser* user);
  IrcUser* get_self() const;
  IrcUser* add_user(const std::string& name,
                    const std::map<char, char>& prefix_to_mode);
  IrcUser* find_user(const std::string& name) const;
  void remove_user(const IrcUser* user);
  void remove_all_users();
  const std::vector<std::unique_ptr<IrcUser>>& get_users() const
  { return this->users; }

protected:
  // Pointer to one IrcUser stored in users
  IrcUser* self{nullptr};
  std::vector<std::unique_ptr<IrcUser>> users{};
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
  DummyIrcChannel(const DummyIrcChannel&) = delete;
  DummyIrcChannel(DummyIrcChannel&&) = delete;
  DummyIrcChannel& operator=(const DummyIrcChannel&) = delete;
  DummyIrcChannel& operator=(DummyIrcChannel&&) = delete;

  /**
   * This flag is at true whenever the user wants to join this channel, but
   * he is not yet connected to the server. When the connection is made, we
   * check that flag and if it’s true, we inform the user that he has just
   * joined that channel.
   * If the user is already connected to the server when he tries to join
   * the channel, we don’t use that flag, we just join it immediately.
   */
  bool joining;
};


