#include <utils/split.hpp>
#include <sstream>

namespace utils
{
  std::vector<std::string> split(const std::string& s, const char delim, const bool allow_empty)
  {
    std::vector<std::string> ret;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
      {
        if (item.empty() && !allow_empty)
          continue ;
        ret.emplace_back(std::move(item));
      }
    return ret;
  }
}
