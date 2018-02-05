#include <utils/scopeguard.hpp>

#include <chrono>

#include <logger/logger.hpp>

template <typename Callback>
auto make_scope_timer(Callback cb)
{
  const auto start_time = std::chrono::steady_clock::now();
  return utils::make_scope_guard([start_time, cb = std::move(cb)]()
                                 {
                                   const auto now = std::chrono::steady_clock::now();
                                   const auto elapsed = now - start_time;
                                   cb(elapsed);
                                 });
}
