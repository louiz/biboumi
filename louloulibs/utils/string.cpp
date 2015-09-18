#include <utils/string.hpp>

bool to_bool(const std::string& val)
{
  return (val == "1" || val == "true");
}
