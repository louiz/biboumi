#ifndef EMPTY_IF_FIXED_SERVER_HPP_INCLUDED
#define EMPTY_IF_FIXED_SERVER_HPP_INCLUDED

#include <string>

#include <config/config.hpp>

namespace utils
{
  inline std::string empty_if_fixed_server(std::string&& str)
  {
    if (!Config::get("fixed_irc_server", "").empty())
      return {};
    return str;
  }

  inline std::string empty_if_fixed_server(const std::string& str)
  {
    if (!Config::get("fixed_irc_server", "").empty())
      return {};
    return str;
  }

}

#endif /* EMPTY_IF_FIXED_SERVER_HPP_INCLUDED */
