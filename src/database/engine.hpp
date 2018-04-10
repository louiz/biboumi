#pragma once

/**
 * Interface to provide non-portable behaviour, specific to each
 * database engine we want to support.
 *
 * Everything else (all portable stuf) should go outside of this class.
 */

#include <database/statement.hpp>

#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <set>

class DatabaseEngine
{
 public:

  DatabaseEngine() = default;
  virtual ~DatabaseEngine() = default;

  DatabaseEngine(const DatabaseEngine&) = delete;
  DatabaseEngine& operator=(const DatabaseEngine&) = delete;
  DatabaseEngine(DatabaseEngine&&) = delete;
  DatabaseEngine& operator=(DatabaseEngine&&) = delete;

  enum class EngineType { None, Postgresql, Sqlite3, };
  virtual EngineType engine_type() const = 0;

  virtual std::map<std::string, std::string> get_all_columns_from_table(const std::string& table_name) = 0;
  virtual std::tuple<bool, std::string> raw_exec(const std::string& query) = 0;
  virtual std::unique_ptr<Statement> prepare(const std::string& query) = 0;
  virtual void extract_last_insert_rowid(Statement& statement) = 0;
  virtual std::string get_returning_id_sql_string(const std::string&)
  {
    return {};
  }
  virtual void convert_date_format(DatabaseEngine&) = 0;
  virtual std::string id_column_type() const = 0;
  virtual std::string datetime_column_type() const = 0;
  virtual long double epoch_to_floating_value(long double seconds) const = 0;
  virtual std::string escape_param_number(int nb) const
  {
    return "$" + std::to_string(nb);
  }
  virtual void init_session()
  {
  }

  int64_t last_inserted_rowid{-1};
};
