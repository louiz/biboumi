#include <irc/irc_channel.hpp>
#include <algorithm>

void IrcChannel::set_self(IrcUser* user)
{
  this->self = user;
}

IrcUser* IrcChannel::add_user(const std::string& name,
                              const std::map<char, char>& prefix_to_mode)
{
  auto new_user = std::make_unique<IrcUser>(name, prefix_to_mode);
  auto old_user = this->find_user(new_user->nick);
  if (old_user)
    return old_user;
  this->users.emplace_back(std::move(new_user));
  return this->users.back().get();
}

IrcUser* IrcChannel::get_self() const
{
  return this->self;
}

IrcUser* IrcChannel::find_user(const std::string& name) const
{
  IrcUser user(name);
  for (const auto& u: this->users)
    {
      if (u->nick == user.nick)
        return u.get();
    }
  return nullptr;
}

void IrcChannel::remove_user(const IrcUser* user)
{
  const auto nick = user->nick;
  const bool is_self = (user == this->self);
  const auto it = std::find_if(this->users.begin(), this->users.end(),
                               [nick](const std::unique_ptr<IrcUser>& u)
                               {
                                 return nick == u->nick;
                               });
  if (it != this->users.end())
    {
      this->users.erase(it);
      if (is_self)
        {
          this->self = nullptr;
          this->joined = false;
        }
    }
}

void IrcChannel::remove_all_users()
{
  this->users.clear();
  this->self = nullptr;
}
