#include <irc/iid.hpp>

Iid::Iid(const std::string& iid)
{
  std::string::size_type sep = iid.find("%");
  if (sep != std::string::npos)
    {
      this->chan = iid.substr(0, sep);
      sep++;
    }
  else
    {
      this->chan = iid;
      return;
    }
  this->server = iid.substr(sep);
}
