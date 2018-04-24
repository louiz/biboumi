#pragma once

#include <database/engine.hpp>

#include <database/statement.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <set>

#include <biboumi.h>

#ifdef SQLITE3_FOUND

#include <sqlite3.h>

class Sqlite3Engine: public DatabaseEngine
{
 public:
  Sqlite3Engine(sqlite3* db);

  ~Sqlite3Engine();

  static std::unique_ptr<DatabaseEngine> open(const std::string& string);

  std::set<std::string> get_all_columns_from_table(const std::string& table_name) override final;
  std::tuple<bool, std::string> raw_exec(const std::string& query) override final;
  std::unique_ptr<Statement> prepare(const std::string& query) override;
  void extract_last_insert_rowid(Statement& statement) override;
  std::string id_column_type() override;
private:
  sqlite3* const db;
};

#else

using namespace std::string_literals;

class Sqlite3Engine
{
public:
  static std::unique_ptr<DatabaseEngine> open(const std::string& string)
  {
    throw std::runtime_error("Cannot open sqlite3 database "s + string + ": biboumi is not compiled with sqlite3 lib.");
    return {};
  }
};

#endif
