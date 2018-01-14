#include <biboumi.h>

#ifdef SQLITE3_FOUND

#include <database/sqlite3_engine.hpp>

#include <database/sqlite3_statement.hpp>

#include <utils/tolower.hpp>
#include <logger/logger.hpp>
#include <vector>

Sqlite3Engine::Sqlite3Engine(sqlite3* db):
    db(db)
{
}

Sqlite3Engine::~Sqlite3Engine()
{
  sqlite3_close(this->db);
}

std::set<std::string> Sqlite3Engine::get_all_columns_from_table(const std::string& table_name)
{
  std::set<std::string> result;
  char* errmsg;
  std::string query{"PRAGMA table_info(" + table_name + ")"};
  int res = sqlite3_exec(this->db, query.data(), [](void* param, int columns_nb, char** columns, char**) -> int {
    constexpr int name_column = 1;
    std::set<std::string>* result = static_cast<std::set<std::string>*>(param);
    if (name_column < columns_nb)
      result->insert(utils::tolower(columns[name_column]));
    return 0;
  }, &result, &errmsg);

  if (res != SQLITE_OK)
    {
      log_error("Error executing ", query, ": ", errmsg);
      sqlite3_free(errmsg);
    }

  return result;
}

std::unique_ptr<DatabaseEngine> Sqlite3Engine::open(const std::string& filename)
{
  sqlite3* new_db;
  auto res = sqlite3_open_v2(filename.data(), &new_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
  if (res != SQLITE_OK)
    {
      log_error("Failed to open database file ", filename, ": ", sqlite3_errmsg(new_db));
      sqlite3_close(new_db);
      throw std::runtime_error("");
    }
  return std::make_unique<Sqlite3Engine>(new_db);
}

std::tuple<bool, std::string> Sqlite3Engine::raw_exec(const std::string& query)
{
  char* error;
  const auto result = sqlite3_exec(db, query.data(), nullptr, nullptr, &error);
  if (result != SQLITE_OK)
    {
      std::string err_msg(error);
      sqlite3_free(error);
      return std::make_tuple(false, err_msg);
    }
  return std::make_tuple(true, std::string{});
}

std::unique_ptr<Statement> Sqlite3Engine::prepare(const std::string& query)
{
  sqlite3_stmt* stmt;
  auto res = sqlite3_prepare(db, query.data(), static_cast<int>(query.size()) + 1,
                             &stmt, nullptr);
  if (res != SQLITE_OK)
    {
      log_error("Error preparing statement: ", sqlite3_errmsg(db));
      return nullptr;
    }
  return std::make_unique<Sqlite3Statement>(stmt);
}

void Sqlite3Engine::extract_last_insert_rowid(Statement&)
{
  this->last_inserted_rowid = sqlite3_last_insert_rowid(this->db);
}

std::string Sqlite3Engine::id_column_type()
{
  return "INTEGER PRIMARY KEY AUTOINCREMENT";
}

#endif
