/**
 * Implementation of the token bucket algorithm.
 *
 * It uses a repetitive TimedEvent, started at construction, to fill the
 * bucket.
 *
 * Every n seconds, it executes the given callback. If the callback
 * returns true, we add a token (if the limit is not yet reached).
 *
 */

#pragma once

#include <utils/timed_events.hpp>
#include <logger/logger.hpp>

class TokensBucket
{
public:
  TokensBucket(long int max_size, std::chrono::milliseconds fill_duration, std::function<bool()> callback, std::string name):
      limit(max_size),
      tokens(limit),
      callback(std::move(callback))
  {
    log_debug("creating TokensBucket with max size: ", max_size);
    TimedEvent event(std::move(fill_duration), [this]() { this->add_token(); }, std::move(name));
    TimedEventsManager::instance().add_event(std::move(event));
  }

  bool use_token()
  {
    if (this->limit < 0)
      return true;
    if (this->tokens > 0)
      {
        this->tokens--;
        return true;
      }
    else
      return false;
  }

  void set_limit(long int limit)
  {
    this->limit = limit;
  }

private:
  long int limit;
  std::size_t tokens;
  std::function<bool()> callback;

  void add_token()
  {
    if (this->limit < 0)
      return;
    if (this->callback() && this->tokens != static_cast<decltype(this->tokens)>(this->limit))
      this->tokens++;
  }
};
