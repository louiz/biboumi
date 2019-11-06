#include <utils/get_first_non_empty.hpp>

template <>
bool is_empty(const std::string& val)
{
  return val.empty();
}

