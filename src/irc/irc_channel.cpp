#include <irc/irc_channel.hpp>
#include <utils/make_unique.hpp>

IrcChannel::IrcChannel():
  joined(false),
  self(nullptr)
{
}

void IrcChannel::set_self(const std::string& name)
{
  this->self = std::make_unique<IrcUser>(name);
}

IrcUser* IrcChannel::add_user(const std::string& name)
{
  this->users.emplace_back(std::make_unique<IrcUser>(name));
  return this->users.back().get();
}

IrcUser* IrcChannel::get_self() const
{
  return this->self.get();
}
