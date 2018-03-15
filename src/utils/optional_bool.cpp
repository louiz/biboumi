#include <utils/optional_bool.hpp>


std::ostream& operator<<(std::ostream& os, const std::optional<bool>& o)
{
  os << std::to_string(o);
  return os;
}
