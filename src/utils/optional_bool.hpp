#pragma once

#include <string>

struct OptionalBool
{
  OptionalBool() = default;

  OptionalBool(bool value):
  is_set(true), value(value) {}

  void set_value(bool value)
  {
    this->is_set = true;
    this->value = value;
  }

  void unset()
  {
    this->is_set = false;
  }

  std::string to_string() const
  {
    if (this->is_set == false)
      return "unset";
    else if (this->value)
      return "true";
    else
      return "false";
  }

  bool is_set{false};
  bool value{false};
};

std::ostream& operator<<(std::ostream& os, const OptionalBool& o);
