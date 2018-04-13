#include <database/select_query.hpp>

template <>
std::string before_column<Database::Date>()
{
  if (Database::engine_type() == DatabaseEngine::EngineType::Sqlite3)
    return "strftime(\"%Y-%m-%dT%H:%M:%SZ\", ";
  else if (Database::engine_type() == DatabaseEngine::EngineType::Postgresql)
    return "to_char(";
  return {};
}

template <>
std::string after_column<Database::Date>()
{
  if (Database::engine_type() == DatabaseEngine::EngineType::Sqlite3)
    return ")";
  else if (Database::engine_type() == DatabaseEngine::EngineType::Postgresql)
    return R"(, 'YYYY-MM-DD"T"HH24:MM:SS"Z"'))";
  return {};
}
