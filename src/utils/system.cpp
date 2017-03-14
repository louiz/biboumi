#include <logger/logger.hpp>
#include <utils/system.hpp>
#include <sys/utsname.h>
#include <cstring>

using namespace std::string_literals;

namespace utils
{
std::string get_system_name()
{
  struct utsname uts;
  const int res = ::uname(&uts);
  if (res == -1)
    {
      log_error("uname failed: ", std::strerror(errno));
      return "Unknown";
    }
  return uts.sysname + " "s + uts.release;
}
}