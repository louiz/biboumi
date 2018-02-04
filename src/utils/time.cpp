#include <utils/time.hpp>
#include <ctime>

#include <sstream>
#include <iomanip>
#include <locale>

#include "biboumi.h"

namespace utils
{
std::string to_string(const std::chrono::system_clock::time_point::rep& time)
{
  constexpr std::size_t stamp_size = 21;
  const std::time_t timestamp = static_cast<std::time_t>(time);
  char date_buf[stamp_size];
  if (std::strftime(date_buf, stamp_size, "%FT%TZ", std::gmtime(&timestamp)) != stamp_size - 1)
    return "";
  return {std::begin(date_buf), std::end(date_buf) - 1};
}

std::time_t parse_datetime(const std::string& stamp)
{
  static const char* format = "%Y-%m-%dT%H:%M:%S";
  std::tm t = {};
#ifdef HAS_GET_TIME
  std::istringstream ss(stamp);
  ss.imbue(std::locale("C"));

  std::string remainings;
  ss >> std::get_time(&t, format) >> remainings;
  if (ss.fail())
    return -1;
#else
                                             /* Y   -   m   -   d   T   H   :   M   :   S */
  constexpr std::size_t stamp_size_without_tz = 4 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2;
  if (!strptime(stamp.data(), format, &t)) {
    return -1;
  }
  const std::string remainings(stamp.data() + stamp_size_without_tz);
#endif

  if (remainings.empty())
    return -1;

  std::string timezone;
  // Skip optional fractions of seconds
  if (remainings[0] == '.')
    {
      const auto pos = remainings.find_first_not_of(".0123456789");
      timezone = remainings.substr(pos);
    }
  else
    timezone = std::move(remainings);

  if (timezone.compare(0, 1, "Z") != 0)
    {
      std::stringstream tz_ss;
      tz_ss << timezone;
      int multiplier = -1;
      char prefix;
      int hours;
      char sep;
      int minutes;
      tz_ss >> prefix >> hours >> sep >> minutes;
      if (tz_ss.fail())
        return -1;
      if (prefix == '-')
        multiplier = +1;
      else if (prefix != '+')
        return -1;

      t.tm_hour += multiplier * hours;
      t.tm_min += multiplier * minutes;
    }
  return ::timegm(&t);
}

}


