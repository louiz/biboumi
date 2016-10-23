#include <xmpp/roster.hpp>

RosterItem::RosterItem(const std::string& jid, const std::string& name,
                       std::vector<std::string>& groups):
  jid(jid),
  name(name),
  groups(groups)
{
}

RosterItem::RosterItem(const std::string& jid, const std::string& name):
  jid(jid),
  name(name),
  groups{}
{
}

void Roster::clear()
{
  this->items.clear();
}
