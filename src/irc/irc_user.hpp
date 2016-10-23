#pragma once


#include <vector>
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

  IrcUser(const IrcUser&) = delete;
  IrcUser(IrcUser&&) = delete;
  IrcUser& operator=(const IrcUser&) = delete;
  IrcUser& operator=(IrcUser&&) = delete;

  void add_mode(const char mode);
  void remove_mode(const char mode);
  char get_most_significant_mode(const std::vector<char>& sorted_user_modes) const;

  std::string nick;
  std::string host;
  std::set<char> modes;
};


