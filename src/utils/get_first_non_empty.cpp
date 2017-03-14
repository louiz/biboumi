#include <utils/get_first_non_empty.hpp>

bool is_empty(const std::string& val)
{
  return val.empty();
}

bool is_empty(const int& val)
{
  return val == 0;
}
