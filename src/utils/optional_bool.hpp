#pragma once

#include <optional>

#include <string>

namespace std
{
inline
std::string to_string(const std::optional<bool> b)
{
  if (!b)
    return "unset";
  else if (*b)
    return "true";
  else
    return "false";
}
}

std::ostream& operator<<(std::ostream& os, const std::optional<bool>& o);
