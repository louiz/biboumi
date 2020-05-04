#pragma once


#include <string>

#include <config/config.hpp>

namespace utils
{
  inline const std::string& empty_if_fixed_server(const std::string& str)
  {
    static const std::string empty{};
    if (!Config::get("fixed_irc_server", "").empty())
      return empty;
    return str;
  }

}


