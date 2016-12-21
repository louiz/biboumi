#pragma once

#include <ctime>
#include <string>

namespace utils
{
std::string to_string(const std::time_t& timestamp);
std::time_t parse_datetime(const std::string& stamp);
}