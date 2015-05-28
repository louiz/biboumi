#ifndef SPLIT_INCLUDED
# define SPLIT_INCLUDED

#include <string>
#include <vector>

namespace utils
{
  std::vector<std::string> split(const std::string &s, const char delim, const bool allow_empty=true);
}

#endif // SPLIT_INCLUDED
