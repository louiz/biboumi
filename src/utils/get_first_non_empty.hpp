#pragma once

#include <string>

bool is_empty(const std::string& val);
bool is_empty(const int& val);

template <typename T>
T get_first_non_empty(T&& last)
{
  return last;
}

template <typename T, typename... Args>
T get_first_non_empty(T&& first, Args&&... args)
{
  if (!is_empty(first))
    return first;
  return get_first_non_empty(std::forward<Args>(args)...);
}
