#include <utils/time.hpp>
#include <time.h>

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

std::time_t parse_datetime(const std::string& stamp)
{
  auto stamp2 = stamp.substr(0, stamp.size() - 1) + "z";
  struct tm tm;
  if (!::strptime(stamp2.data(), "%FT%T%Z", &tm))
    return -1;
  auto res = ::timegm(&tm);
  return res;
}

}
