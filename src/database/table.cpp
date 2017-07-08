#include <database/table.hpp>

std::set<std::string> get_all_columns_from_table(sqlite3* db, const std::string& table_name)
{
  std::set<std::string> result;
  char* errmsg;
  std::string query{"PRAGMA table_info(" + table_name + ")"};
  log_debug(query);
  int res = sqlite3_exec(db, query.data(), [](void* param, int columns_nb, char** columns, char**) -> int {
    constexpr int name_column = 1;
    std::set<std::string>* result = static_cast<std::set<std::string>*>(param);
    log_debug("Table has column ", columns[name_column]);
    if (name_column < columns_nb)
      result->insert(columns[name_column]);
    return 0;
  }, &result, &errmsg);

  if (res != SQLITE_OK)
    {
      log_error("Error executing ", query, ": ", errmsg);
      sqlite3_free(errmsg);
    }

  return result;
}
