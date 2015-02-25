#include <utils/tolower.hpp>
#include <config/config.hpp>

#include <irc/iid.hpp>

Iid::Iid(const std::string& iid):
  is_channel(false),
  is_user(false)
{
  const std::string fixed_irc_server = Config::get("fixed_irc_server", "");
  if (fixed_irc_server.empty())
    this->init(iid);
  else
    this->init_with_fixed_server(iid, fixed_irc_server);
}


void Iid::init(const std::string& iid)
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

void Iid::init_with_fixed_server(const std::string& iid, const std::string& hostname)
{
  this->set_server(hostname);

  const std::string::size_type sep = iid.find("!");

  // Without any separator, we consider that it's a channel
  if (sep == std::string::npos)
    {
      this->is_channel = true;
      this->set_local(iid);
    }
  else // A separator can be present to differenciate a channel from a user,
       // but the part behind it (the hostname) is ignored
    {
      this->set_local(iid.substr(0, sep));
        this->is_user = true;
    }
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
    if (Config::get("fixed_irc_server", "").empty())
      return iid.get_local() + iid.get_sep() + iid.get_server();
    else
      {
        if (iid.get_sep() == "!")
          return iid.get_local() + iid.get_sep();
        else
          return iid.get_local();
      }
  }
}
