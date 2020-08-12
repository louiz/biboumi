#pragma once

#include <functional>

struct Capability
{
  std::function<void()> on_ack;
  std::function<void()> on_nack;
};
