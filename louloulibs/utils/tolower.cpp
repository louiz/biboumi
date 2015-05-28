#include <utils/tolower.hpp>

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
