#ifndef TOLOWER_INCLUDED
# define TOLOWER_INCLUDED

#include <string>

namespace utils
{
  std::string tolower(const std::string& original)
  {
    std::string res;
    res.reserve(original.size());
    for (const char c: original)
      res += static_cast<char>(std::tolower(c));
    return res;
  }
}

#endif // SPLIT_INCLUDED
