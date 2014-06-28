#ifndef TIMED_EVENTS_HPP
# define TIMED_EVENTS_HPP

#include <functional>
#include <string>
#include <chrono>
#include <list>

using namespace std::literals::chrono_literals;

namespace utils {
static constexpr std::chrono::milliseconds no_timeout = std::chrono::milliseconds(-1);
}

class TimedEventsManager;

/**
 * A callback with an associated date.
 */

class TimedEvent
{
  friend class TimedEventsManager;
public:
  /**
   * An event the occurs only once, at the given time_point
   */
  explicit TimedEvent(std::chrono::steady_clock::time_point&& time_point,
                      std::function<void()> callback, const std::string& name="");
  explicit TimedEvent(std::chrono::milliseconds&& duration,
                      std::function<void()> callback, const std::string& name="");

  explicit TimedEvent(TimedEvent&&);
  ~TimedEvent();
  /**
   * Whether or not this event happens after the other one.
   */
  bool is_after(const TimedEvent& other) const;
  bool is_after(const std::chrono::steady_clock::time_point& time_point) const;
  /**
   * Return the duration difference between now and the event time point.
   * If the difference would be negative (i.e. the event is expired), the
   * returned value is 0 instead. The value cannot then be negative.
   */
  std::chrono::milliseconds get_timeout() const;
  void execute();
  const std::string& get_name() const;

private:
  /**
   * The next time point at which the event is executed.
   */
  std::chrono::steady_clock::time_point time_point;
  /**
   * The function to execute.
   */
  const std::function<void()> callback;
  /**
   * Whether or not this events repeats itself until it is destroyed.
   */
  const bool repeat;
  /**
   * This value is added to the time_point each time the event is executed,
   * if repeat is true. Otherwise it is ignored.
   */
  const std::chrono::milliseconds repeat_delay;
  /**
   * A name that is used to identify that event. If you want to find your
   * event (for example if you want to cancel it), the name should be
   * unique.
   */
  const std::string name;

  TimedEvent(const TimedEvent&) = delete;
  TimedEvent& operator=(const TimedEvent&) = delete;
  TimedEvent& operator=(TimedEvent&&) = delete;
};

/**
 * A class managing a list of TimedEvents.
 * They are sorted, new events can be added, removed, fetch, etc.
 */

class TimedEventsManager
{
public:
  ~TimedEventsManager();
  /**
   * Return the unique instance of this class
   */
  static TimedEventsManager& instance();
  /**
   * Add an event to the list of managed events. The list is sorted after
   * this call.
   */
  void add_event(TimedEvent&& event);
  /**
   * Returns the duration, in milliseconds, between now and the next
   * available event. If the event is already expired (the duration is
   * negative), 0 is returned instead (as in “it's not too late, execute it
   * now”)
   * Returns a negative value if no event is available.
   */
  std::chrono::milliseconds get_timeout() const;
  /**
   * Execute all the expired events (if their expiration time is exactly
   * now, or before now). The event is then removed from the list. If the
   * event does repeat, its expiration time is updated and it is reinserted
   * in the list at the correct position.
   * Returns the number of executed events.
   */
  std::size_t execute_expired_events();
  /**
   * Remove (and thus cancel) all the timed events with the given name.
   * Returns the number of canceled events.
   */
  std::size_t cancel(const std::string& name);
  /**
   * Return the number of managed events.
   */
  std::size_t size() const;

private:
  explicit TimedEventsManager();
  std::list<TimedEvent> events;
  TimedEventsManager(const TimedEventsManager&) = delete;
  TimedEventsManager(TimedEventsManager&&) = delete;
  TimedEventsManager& operator=(const TimedEventsManager&) = delete;
  TimedEventsManager& operator=(TimedEventsManager&&) = delete;
};

#endif // TIMED_EVENTS_HPP
