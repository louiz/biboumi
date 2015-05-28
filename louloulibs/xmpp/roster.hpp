#ifndef ROSTER_HPP_INCLUDED
#define ROSTER_HPP_INCLUDED

#include <algorithm>
#include <string>
#include <vector>

class RosterItem
{
public:
  RosterItem(const std::string& jid, const std::string& name,
             std::vector<std::string>& groups);
  RosterItem(const std::string& jid, const std::string& name);
  RosterItem() = default;
  ~RosterItem() = default;
  RosterItem(const RosterItem&) = default;
  RosterItem(RosterItem&&) = default;
  RosterItem& operator=(const RosterItem&) = default;
  RosterItem& operator=(RosterItem&&) = default;

  std::string jid;
  std::string name;
  std::vector<std::string> groups;

private:
};

/**
 * Keep track of the last known stat of a JID's roster
 */
class Roster
{
public:
  Roster() = default;
  ~Roster() = default;

  void clear();

  template <typename... ArgsType>
  RosterItem* add_item(ArgsType&&... args)
  {
    this->items.emplace_back(std::forward<ArgsType>(args)...);
    auto it = this->items.end() - 1;
    return &*it;
  }
  RosterItem* get_item(const std::string& jid)
  {
    auto it = std::find_if(this->items.begin(), this->items.end(),
                           [this, &jid](const auto& item)
                           {
                             return item.jid == jid;
                           });
    if (it != this->items.end())
      return &*it;
    return nullptr;
  }
  const std::vector<RosterItem>& get_items() const
  {
    return this->items;
  }

private:
  std::vector<RosterItem> items;

  Roster(const Roster&) = delete;
  Roster(Roster&&) = delete;
  Roster& operator=(const Roster&) = delete;
  Roster& operator=(Roster&&) = delete;
};

#endif /* ROSTER_HPP_INCLUDED */
