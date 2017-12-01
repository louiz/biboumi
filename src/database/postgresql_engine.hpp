#pragma once

#include <biboumi.h>
#include <string>
#include <stdexcept>
#include <memory>

#include <database/statement.hpp>
#include <database/engine.hpp>

#include <tuple>
#include <set>

#ifdef PQ_FOUND

#include <libpq-fe.h>

class PostgresqlEngine: public DatabaseEngine
{
 public:
  PostgresqlEngine(PGconn*const conn);

  ~PostgresqlEngine();

  static std::unique_ptr<DatabaseEngine> open(const std::string& string);

  std::set<std::string> get_all_columns_from_table(const std::string& table_name) override final;
  std::tuple<bool, std::string> raw_exec(const std::string& query) override final;
  std::unique_ptr<Statement> prepare(const std::string& query) override;
  void extract_last_insert_rowid(Statement& statement) override;
  std::string get_returning_id_sql_string(const std::string& col_name) override;
  std::string id_column_type() override;
private:
  PGconn* const conn;
};

#else

class PostgresqlEngine
{
public:
  static std::unique_ptr<DatabaseEngine> open(const std::string& string)
  {
    throw std::runtime_error("Cannot open postgresql database "s + string + ": biboumi is not compiled with libpq.");
  }
};

#endif
