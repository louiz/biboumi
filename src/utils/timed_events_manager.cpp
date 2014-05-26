#include <utils/timed_events.hpp>

TimedEventsManager::TimedEventsManager()
{
}

TimedEventsManager::~TimedEventsManager()
{
}

void TimedEventsManager::add_event(TimedEvent&& event)
{
  for (auto it = this->events.begin(); it != this->events.end(); ++it)
    {
      if (it->is_after(event))
        {
          this->events.emplace(it, std::move(event));
          return;
        }
    }
  this->events.emplace_back(std::move(event));
}

std::chrono::milliseconds TimedEventsManager::get_timeout() const
{
  if (this->events.empty())
    return utils::no_timeout;
  return this->events.front().get_timeout() + std::chrono::milliseconds(1);
}

std::size_t TimedEventsManager::execute_expired_events()
{
  std::size_t count = 0;
  const auto now = std::chrono::steady_clock::now();
  for (auto it = this->events.begin(); it != this->events.end();)
    {
      if (!it->is_after(now))
        {
          TimedEvent copy(std::move(*it));
          it = this->events.erase(it);
          ++count;
          copy.execute();
          if (copy.repeat)
            {
              copy.time_point += copy.repeat_delay;
              this->add_event(std::move(copy));
            }
          continue;
        }
      else
        break;
    }
  return count;
}

