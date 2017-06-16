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

  bool is_set{false};
  bool value{false};
};
