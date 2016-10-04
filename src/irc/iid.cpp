#include <utils/tolower.hpp>
#include <config/config.hpp>
#include <bridge/bridge.hpp>
#include <irc/iid.hpp>

#include <utils/encoding.hpp>

constexpr char Iid::separator[];

Iid::Iid(const std::string& local, const std::string& server, Iid::Type type):
        type(type),
        local(local),
        server(server)
{
}

Iid::Iid(const std::string& iid, const std::set<char>& chantypes)
{
  this->init(iid);
  this->set_type(std::set<char>(chantypes));
}

Iid::Iid(const std::string& iid, const std::initializer_list<char>& chantypes):
    Iid(iid, std::set<char>(chantypes))
{
}

Iid::Iid(const std::string& iid, const Bridge *bridge)
{
  this->init(iid);
  const auto chantypes = bridge->get_chantypes(this->server);
  this->set_type(chantypes);
}

void Iid::set_type(const std::set<char>& chantypes)
{
  if (this->local.empty())
    return;

  if (chantypes.count(this->local[0]) == 1)
    this->type = Iid::Type::Channel;
  else
    this->type = Iid::Type::User;
}

void Iid::init(const std::string& iid)
{
  const std::string fixed_irc_server = Config::get("fixed_irc_server", "");

  if (fixed_irc_server.empty())
  {
    const std::string::size_type sep = iid.find('%');
    if (sep != std::string::npos)
    {
      this->set_local(iid.substr(0, sep));
      this->set_server(iid.substr(sep + 1));
      this->type = Iid::Type::Channel;
    }
    else
    {
      this->set_server(iid);
      this->type = Iid::Type::Server;
    }
  }
  else
  {
    this->set_server(fixed_irc_server);
    this->set_local(iid);
  }
}

void Iid::set_local(const std::string& loc)
{
  std::string local(utils::tolower(loc));
  xep0106::decode(local);
  this->local = local;
}

void Iid::set_server(const std::string& serv)
{
  this->server = utils::tolower(serv);
}

const std::string& Iid::get_local() const
{
  return this->local;
}

const std::string Iid::get_encoded_local() const
{
  std::string local(this->local);
  xep0106::encode(local);
  return local;
}

const std::string& Iid::get_server() const
{
  return this->server;
}

namespace std {
  const std::string to_string(const Iid& iid)
  {
    if (Config::get("fixed_irc_server", "").empty())
    {
      if (iid.type == Iid::Type::Server)
        return iid.get_server();
      else
        return iid.get_encoded_local() + iid.separator + iid.get_server();
    }
    else
      return iid.get_encoded_local();
  }
}

std::tuple<std::string, std::string> Iid::to_tuple() const
{
  return std::make_tuple(this->get_local(), this->get_server());
}
