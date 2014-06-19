#include <utils/tolower.hpp>

#include <irc/iid.hpp>

Iid::Iid(const std::string& iid):
  is_channel(false),
  is_user(false)
{
  const std::string::size_type sep = iid.find_first_of("%!");
  if (sep != std::string::npos)
    {
      if (iid[sep] == '%')
        this->is_channel = true;
      else
        this->is_user = true;
      this->set_local(iid.substr(0, sep));
      this->set_server(iid.substr(sep + 1));
    }
  else
    this->set_server(iid);
}

Iid::Iid(const Iid& other):
  is_channel(other.is_channel),
  is_user(other.is_user),
  local(other.local),
  server(other.server)
{
}

Iid::Iid():
  is_channel(false),
  is_user(false)
{
}

void Iid::set_local(const std::string& loc)
{
  this->local = utils::tolower(loc);
}

void Iid::set_server(const std::string& serv)
{
  this->server = utils::tolower(serv);
}

const std::string& Iid::get_local() const
{
  return this->local;
}

const std::string& Iid::get_server() const
{
  return this->server;
}

std::string Iid::get_sep() const
{
  if (this->is_channel)
    return "%";
  else if (this->is_user)
    return "!";
  return "";
}

namespace std {
  const std::string to_string(const Iid& iid)
  {
    return iid.get_local() + iid.get_sep() + iid.get_server();
  }
}
