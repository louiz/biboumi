#include <biboumi.h>

#ifdef SQLITE3_FOUND

#include <database/database.hpp>
#include <database/sqlite3_engine.hpp>

#include <database/sqlite3_statement.hpp>

#include <database/query.hpp>

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

std::map<std::string, std::string> Sqlite3Engine::get_all_columns_from_table(const std::string& table_name)
{
  std::map<std::string, std::string> result;
  char* errmsg;
  std::string query{"PRAGMA table_info(" + table_name + ")"};
  int res = sqlite3_exec(this->db, query.data(), [](void* param, int columns_nb, char** columns, char**) -> int {
    constexpr int name_column = 1;
    constexpr int data_type_column = 2;
    auto* result = static_cast<std::map<std::string, std::string>*>(param);
    if (name_column < columns_nb && data_type_column < columns_nb)
      (*result)[utils::tolower(columns[name_column])] = utils::tolower(columns[data_type_column]);
    return 0;
  }, &result, &errmsg);

  if (res != SQLITE_OK)
    {
      log_error("Error executing ", query, ": ", errmsg);
      sqlite3_free(errmsg);
    }

  return result;
}

template <typename... T>
static auto make_select_query(const Row<T...>&, const std::string& name)
{
  return SelectQuery<T...>{name};
}

void Sqlite3Engine::convert_date_format(DatabaseEngine& db)
{
  Transaction transaction{};
  auto rollback = [&transaction] (const std::string& error_msg)
  {
    log_error("Failed to execute query: ", error_msg);
    transaction.rollback();
  };

  const auto real_name = Database::muc_log_lines.get_name();
  const auto tmp_name = real_name + "tmp_";
  const std::string date_name = Database::Date::name;

  auto result = db.raw_exec("ALTER TABLE " + real_name + " RENAME TO " + tmp_name);
  if (!std::get<bool>(result))
    return rollback(std::get<std::string>(result));

  Database::muc_log_lines.create(db);

  Database::OldMucLogLineTable old_muc_log_line(tmp_name);
  auto select_query = make_select_query(old_muc_log_line.row(), old_muc_log_line.get_name());

  auto& select_body = select_query.body;
  auto begin = select_body.find(date_name);
  select_body.replace(begin, date_name.size(), "julianday("+date_name+", 'unixepoch')");
  select_body = "INSERT INTO " + real_name + " " + select_body;

  result = db.raw_exec(select_body);
  if (!std::get<bool>(result))
    return rollback(std::get<std::string>(result));

  result = db.raw_exec("DROP TABLE " + tmp_name);
  if (!std::get<bool>(result))
    return rollback(std::get<std::string>(result));
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
#ifdef DEBUG_SQL_QUERIES
  log_debug("SQL QUERY: ", query);
  const auto timer = make_sql_timer();
#endif

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

std::string Sqlite3Engine::id_column_type() const
{
  return "INTEGER PRIMARY KEY AUTOINCREMENT";
}

std::string Sqlite3Engine::datetime_column_type() const
{
  return "REAL";
}

long double Sqlite3Engine::epoch_to_floating_value(long double d) const
{
  return (d / 86400.0) + 2440587.5;
}

#endif
