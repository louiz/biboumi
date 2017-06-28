#pragma once

#include <database/select_query.hpp>
#include <database/type_to_sql.hpp>
#include <logger/logger.hpp>
#include <database/row.hpp>

#include <algorithm>
#include <string>
#include <set>

using namespace std::string_literals;

std::set<std::string> get_all_columns_from_table(sqlite3* db, const std::string& table_name);

template <typename ColumnType>
void add_column_to_table(sqlite3* db, const std::string& table_name)
{
  const std::string name = ColumnType::name;
  std::string query{"ALTER TABLE "s + table_name + " ADD " + ColumnType::name + " " + TypeToSQLType<typename ColumnType::real_type>::type};
  log_debug(query);
  char* error;
  const auto result = sqlite3_exec(db, query.data(), nullptr, nullptr, &error);
  if (result != SQLITE_OK)
    {
      log_error("Error adding column ", name, " to table ", table_name, ": ",  error);
      sqlite3_free(error);
    }
}

template <typename... T>
class Table
{
  static_assert(sizeof...(T) > 0, "Table cannot be empty");
  using ColumnTypes = std::tuple<T...>;

 public:
  using RowType = Row<T...>;

  Table(std::string name):
      name(std::move(name))
  {}

  void upgrade(sqlite3* db)
  {
    const auto existing_columns = get_all_columns_from_table(db, this->name);
    add_column_if_not_exists(db, existing_columns);
  }

  void create(sqlite3* db)
  {
    std::string res{"CREATE TABLE IF NOT EXISTS "};
    res += this->name;
    res += " (\n";
    this->add_column_create(res);
    res += ")";

    log_debug(res);

    char* error;
    const auto result = sqlite3_exec(db, res.data(), nullptr, nullptr, &error);
    log_debug("result: ", +result);
    if (result != SQLITE_OK)
      {
        log_error("Error executing query: ", error);
        sqlite3_free(error);
      }
  }

  RowType row()
  {
    return {this->name};
  }

  SelectQuery<T...> select()
  {
    SelectQuery<T...> select(this->name);
    return select;
  }

  const std::string& get_name() const
  {
    return this->name;
  }

 private:

  template <std::size_t N=0>
  typename std::enable_if<N < sizeof...(T), void>::type
  add_column_if_not_exists(sqlite3* db, const std::set<std::string>& existing_columns)
  {
    using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<ColumnTypes>()))>::type;
    if (existing_columns.count(ColumnType::name) != 1)
      {
        add_column_to_table<ColumnType>(db, this->name);
      }
    add_column_if_not_exists<N+1>(db, existing_columns);
  }
  template <std::size_t N=0>
  typename std::enable_if<N == sizeof...(T), void>::type
  add_column_if_not_exists(sqlite3*, const std::set<std::string>&)
  {}

  template <std::size_t N=0>
  typename std::enable_if<N < sizeof...(T), void>::type
  add_column_create(std::string& str)
  {
    using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<ColumnTypes>()))>::type;
    using RealType = typename ColumnType::real_type;
    str += ColumnType::name;
    str += " ";
    str += TypeToSQLType<RealType>::type;
    str += " "s + ColumnType::options;
    if (N != sizeof...(T) - 1)
      str += ",";
    str += "\n";

    add_column_create<N+1>(str);
  }

  template <std::size_t N=0>
  typename std::enable_if<N == sizeof...(T), void>::type
  add_column_create(std::string&)
  { }

  const std::string name;
};
