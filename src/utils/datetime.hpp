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

private:
  std::string s;
  time_point t;
};
