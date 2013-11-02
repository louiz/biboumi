#ifndef SCOPEGUARD_HPP
#define SCOPEGUARD_HPP

#include <functional>
#include <vector>

/**
 * A class to be used to make sure some functions are called when the scope
 * is left, because they will be called in the ScopeGuard's destructor.  It
 * can for example be used to delete some pointer whenever any exception is
 * called.  Example:

 * {
 *  ScopeGuard scope;
 *  int* number = new int(2);
 *  scope.add_callback([number]() { delete number; });
 *  // Do some other stuff with the number. But these stuff might throw an exception:
 *  throw std::runtime_error("Some error not caught here, but in our caller");
 *  return true;
 * }

 * In this example, our pointer will always be deleted, even when the
 * exception is thrown.  If we want the functions to be called only when the
 * scope is left because of an unexpected exception, we can use
 * ScopeGuard::disable();
 */

namespace utils
{

class ScopeGuard
{
public:
  /**
   * The constructor can take a callback. But additional callbacks can be
   * added later with add_callback()
   */
  explicit ScopeGuard(std::function<void()>&& func):
    enabled(true)
  {
    this->add_callback(std::move(func));
  }
  /**
   * default constructor, the scope guard is enabled but empty, use
   * add_callback()
   */
  explicit ScopeGuard():
    enabled(true)
  {
  }
  /**
   * Call all callbacks in the desctructor, unless it has been disabled.
   */
  ~ScopeGuard()
  {
    if (this->enabled)
      for (auto& func: this->callbacks)
        func();
  }
  /**
   * Add a callback to be called in our destructor, one scope guard can be
   * used for more than one task, if needed.
   */
  void add_callback(std::function<void()>&& func)
  {
    this->callbacks.emplace_back(std::move(func));
  }
  /**
   * Disable that scope guard, nothing will be done when the scope is
   * exited.
   */
  void disable()
  {
    this->enabled = false;
  }

private:
  bool enabled;
  std::vector<std::function<void()>> callbacks;

  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(ScopeGuard&&) = delete;
  ScopeGuard(ScopeGuard&&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
};

}

#endif
