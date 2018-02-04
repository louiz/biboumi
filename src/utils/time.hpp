#pragma once

#include <ctime>
#include <string>
#include <chrono>

namespace utils
{
std::string to_string(const std::chrono::system_clock::time_point::rep& timestamp);
std::time_t parse_datetime(const std::string& stamp);
}
