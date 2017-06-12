#pragma once

#include <database/row.hpp>
#include <database/select_query.hpp>
#include <database/type_to_sql.hpp>

#include <string>
#include <algorithm>
#include <iostream>

template <typename... T>
class Table
{
  static_assert(sizeof...(T) > 0, "Table cannot be empty");
  using ColumnTypes = std::tuple<T...>;

 public:
  using RowType = Row<T...>;

  Table(sqlite3* db, std::string name):
      db(db),
      name(std::move(name))
  {}

  void create()
  {
    std::string res{"CREATE TABLE "};
    res += this->name;
    res += " (\n";
    this->add_column_create<0>(res);
    res += ")";

    std::cout << res << std::endl;

    char* error;
    const auto result = sqlite3_exec(this->db, res.data(), nullptr, nullptr, &error);
    if (result != SQLITE_OK)
      {
        std::cout << "Error executing query: " << error;
        sqlite3_free(error);
      }
    else
      {
        std::cout << "success." << std::endl;
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

  sqlite3* db;
  const std::string name;
  std::tuple<T...> columns;
  std::array<std::string, sizeof...(T)> types;
};
