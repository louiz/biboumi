#include <database/insert_query.hpp>

template <>
std::string before_value<Database::Date>()
{
  if (Database::engine_type() == DatabaseEngine::EngineType::Sqlite3)
    return "julianday(";
  if (Database::engine_type() == DatabaseEngine::EngineType::Postgresql)
    return "to_timestamp(";
  return {};
}

template <>
std::string after_value<Database::Date>()
{
  if (Database::engine_type() == DatabaseEngine::EngineType::Sqlite3)
    return ", \"unixepoch\")";
  if (Database::engine_type() == DatabaseEngine::EngineType::Postgresql)
    return ")";
  return {};
}
