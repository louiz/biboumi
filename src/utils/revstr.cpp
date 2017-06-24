#include <utils/revstr.hpp>

namespace utils
{
  std::string revstr(const std::string& original)
  {
    return {original.rbegin(), original.rend()};
  }
}
