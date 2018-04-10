#pragma once

#include <utils/datetime.hpp>
#include <database/engine.hpp>

#include <logger/logger.hpp>
#include <database/postgresql_engine.hpp>
#include <database/sqlite3_engine.hpp>

class DatetimeWriter
{
public:
  DatetimeWriter(DateTime datetime, const DatabaseEngine& engine):
      datetime(datetime),
      engine(engine)
  {}

  long double get_value() const
  {
    const long double epoch_duration = this->datetime.epoch().count();
    const long double epoch_seconds = epoch_duration / std::chrono::system_clock::period::den;
    return this->engine.epoch_to_floating_value(epoch_seconds);
  }
  std::string escape_param_number(int value) const
  {
    return this->engine.escape_param_number(value);
  }

private:
  const DateTime datetime;
  const DatabaseEngine& engine;
};
