#include <utils/time.hpp>
#include <time.h>

#include <sstream>
#include <iomanip>
#include <locale>

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
  static const char* format = "%Y-%m-%dT%H:%M:%S";
  std::tm t = {};
  std::istringstream ss(stamp);
  ss.imbue(std::locale("en_US.utf-8"));

  std::string timezone;
  ss >> std::get_time(&t, format) >> timezone;
  if (ss.fail())
    return -1;

  if (timezone.empty())
    return -1;

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


