#ifndef SPLIT_INCLUDED
# define SPLIT_INCLUDED

#include <string>
#include <sstream>
#include <vector>

namespace utils
{
  std::vector<std::string> split(const std::string &s, const char delim)
  {
    std::vector<std::string> ret;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
      ret.emplace_back(std::move(item));
    return ret;
  }
}

#endif // SPLIT_INCLUDED
