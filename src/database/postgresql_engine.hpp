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
  EngineType engine_type() const override
  {
    return EngineType::Postgresql;
  }

  std::map<std::string, std::string> get_all_columns_from_table(const std::string& table_name) override final;
  std::tuple<bool, std::string> raw_exec(const std::string& query) override final;
  std::unique_ptr<Statement> prepare(const std::string& query) override;
  void extract_last_insert_rowid(Statement& statement) override;
  std::string get_returning_id_sql_string(const std::string& col_name) override;
  std::string id_column_type() const override;
  std::string datetime_column_type() const override;
  void convert_date_format(DatabaseEngine& engine) override;
  long double epoch_to_floating_value(long double seconds) const override;
  void init_session() override;
  std::string escape_param_number(int nb) const override;
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
