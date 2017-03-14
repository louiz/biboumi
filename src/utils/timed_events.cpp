#include <utils/timed_events.hpp>

TimedEvent::TimedEvent(std::chrono::steady_clock::time_point&& time_point,
                       std::function<void()> callback, const std::string& name):
  time_point(std::move(time_point)),
  callback(callback),
  repeat(false),
  repeat_delay(0),
  name(name)
{
}

TimedEvent::TimedEvent(std::chrono::milliseconds&& duration,
                       std::function<void()> callback, const std::string& name):
  time_point(std::chrono::steady_clock::now() + duration),
  callback(callback),
  repeat(true),
  repeat_delay(std::move(duration)),
  name(name)
{
}

bool TimedEvent::is_after(const TimedEvent& other) const
{
  return this->is_after(other.time_point);
}

bool TimedEvent::is_after(const std::chrono::steady_clock::time_point& time_point) const
{
  return this->time_point > time_point;
}

std::chrono::milliseconds TimedEvent::get_timeout() const
{
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(this->time_point - std::chrono::steady_clock::now());
  return std::max(diff, 0ms);
}

void TimedEvent::execute() const
{
  this->callback();
}

const std::string& TimedEvent::get_name() const
{
  return this->name;
}
