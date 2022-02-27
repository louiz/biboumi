#include "catch2/catch.hpp"

#include <utils/timed_events.hpp>

/**
 * Let Catch know how to display std::chrono::duration values
 */
namespace Catch
{
  template<typename Rep, typename Period> struct StringMaker<std::chrono::duration<Rep, Period>>
  {
    static std::string convert(const std::chrono::duration<Rep, Period>& value)
    {
      return  std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(value).count()) + "ms";
    }
  };
}

/**
 * TODO, use a mock clock instead of relying on the real time with a sleep:
 * it’s unreliable on heavy load.
 */
#include <thread>

TEST_CASE("Test timed event expiration")
{
  SECTION("Check what happens when there is no events")
    {
      CHECK(TimedEventsManager::instance().get_timeout() == utils::no_timeout);
      CHECK(TimedEventsManager::instance().execute_expired_events() == 0);
    }

  // Add a single event
  TimedEventsManager::instance().add_event(TimedEvent(std::chrono::steady_clock::now() + 50ms, [](){}));

  // The event should not yet be expired
  CHECK(TimedEventsManager::instance().get_timeout() > 0ms);
  CHECK(TimedEventsManager::instance().execute_expired_events() == 0);

  std::chrono::milliseconds timoute = TimedEventsManager::instance().get_timeout();
  std::this_thread::sleep_for(timoute + 1ms);

  // Event is now expired
  CHECK(TimedEventsManager::instance().execute_expired_events() == 1);
  CHECK(TimedEventsManager::instance().get_timeout() == utils::no_timeout);
}

TEST_CASE("Test timed event cancellation")
{
  auto now = std::chrono::steady_clock::now();
  TimedEventsManager::instance().add_event(TimedEvent(now + 100ms, [](){ }, "un"));
  TimedEventsManager::instance().add_event(TimedEvent(now + 200ms, [](){ }, "deux"));
  TimedEventsManager::instance().add_event(TimedEvent(now + 300ms, [](){ }, "deux"));

  CHECK(TimedEventsManager::instance().get_timeout() > 0ms);
  CHECK(TimedEventsManager::instance().size() == 3);
  CHECK(TimedEventsManager::instance().cancel("un") == 1);
  CHECK(TimedEventsManager::instance().size() == 2);
  CHECK(TimedEventsManager::instance().cancel("deux") == 2);
  CHECK(TimedEventsManager::instance().get_timeout() == utils::no_timeout);
}
