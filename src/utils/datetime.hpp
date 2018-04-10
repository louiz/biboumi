#pragma once

#include <chrono>
#include <string>

#include <logger/logger.hpp>

class DateTime
{
public:
  enum class Engine {
    Postgresql,
    Sqlite3,
  } engine{Engine::Sqlite3};

  using time_point = std::chrono::system_clock::time_point;

  DateTime():
      s{},
      t{}
  { }

  DateTime(std::time_t t):
      t(std::chrono::seconds(t))
  {}

  DateTime(std::string s):
      s(std::move(s))
  {}

  DateTime& operator=(const std::string& s)
  {
    this->s = s;
    return *this;
  }

  DateTime& operator=(const time_point t)
  {
    this->t = t;
    return *this;
  }

  const std::string& to_string() const
  {
    return this->s;
  }

  time_point::duration epoch() const
  {
    return this->t.time_since_epoch();
  }

  long double julianday() const
  {
    log_debug("ici?");
    auto res = ((static_cast<long double>(this->epoch().count()) / std::chrono::system_clock::period::den) / 86400) + 2440587.5;
    return res;
  }

private:
  std::string s;
  time_point t;
};

inline long double to_julianday(std::time_t t)
{
  return static_cast<long double>(t) / 86400.0 + 2440587.5;
}
