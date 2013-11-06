#include <xmpp/jid.hpp>

Jid::Jid(const std::string& jid)
{
  std::string::size_type at = jid.find("@");
  if (at != std::string::npos)
    {
      this->local = jid.substr(0, at);
      at++;
    }
  else
    at = 0;
  std::string::size_type slash = jid.find("/", at);
  if (slash != std::string::npos)
    {
      this->resource = jid.substr(slash + 1);
    }
  this->domain = jid.substr(at, slash - at);
}
