#include <utils/time.hpp>

namespace utils
{
std::string to_string(const std::time_t& timestamp)
{
    constexpr std::size_t stamp_size = 20;
    char date_buf[stamp_size];
    std::strftime(date_buf, stamp_size, "%FT%TZ", std::gmtime(&timestamp));
    return {std::begin(date_buf), std::end(date_buf)};
}
}