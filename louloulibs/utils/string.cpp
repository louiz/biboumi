#include <utils/string.hpp>

bool to_bool(const std::string& val)
{
  return (val == "1" || val == "true");
}

std::vector<std::string> cut(const std::string& val, const std::size_t size)
{
  std::vector<std::string> res;
  std::string::size_type pos = 0;
  while (pos < val.size())
    {
      res.emplace_back(val.substr(pos, size));
      pos += size;
    }
  return res;
}
