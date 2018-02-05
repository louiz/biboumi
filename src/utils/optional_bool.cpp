#include <utils/optional_bool.hpp>


std::ostream& operator<<(std::ostream& os, const OptionalBool& o)
{
  os << o.to_string();
  return os;
}
