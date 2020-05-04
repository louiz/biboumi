#pragma once

#include <string>

template <typename T>
bool is_empty(const T& val)
{
  return val == 0;
}
template <>
bool is_empty(const std::string& val);

template <typename T>
T& get_first_non_empty(T&& last)
{
  return last;
}

template <typename T, typename... Args>
T& get_first_non_empty(T&& first, Args&&... args)
{
  if (!is_empty(first))
    return first;
  return get_first_non_empty(std::forward<Args>(args)...);
}
