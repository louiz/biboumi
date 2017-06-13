#pragma once

#include <database/select_query.hpp>
#include <database/type_to_sql.hpp>
#include <logger/logger.hpp>
#include <database/row.hpp>

#include <algorithm>
#include <string>

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

  void create(sqlite3* db)
  {
    std::string res{"CREATE TABLE IF NOT EXISTS "};
    res += this->name;
    res += " (\n";
    this->add_column_create<0>(res);
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
  template <std::size_t N>
  typename std::enable_if<N < sizeof...(T), void>::type
  add_column_create(std::string& str)
  {
    using ColumnType = typename std::remove_reference<decltype(std::get<N>(this->columns))>::type;
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

  template <std::size_t N>
  typename std::enable_if<N == sizeof...(T), void>::type
  add_column_create(std::string&)
  { }

  const std::string name;
  std::tuple<T...> columns;
  std::array<std::string, sizeof...(T)> types;
};
