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
