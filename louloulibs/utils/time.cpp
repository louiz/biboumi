#include <utils/time.hpp>

namespace utils
{
std::string to_string(const std::time_t& timestamp)
{
  constexpr std::size_t stamp_size = 21;
  char date_buf[stamp_size];
  if (std::strftime(date_buf, stamp_size, "%FT%TZ", std::gmtime(&timestamp)) != stamp_size - 1)
    return "";
  return {std::begin(date_buf), std::end(date_buf) - 1};
}
}