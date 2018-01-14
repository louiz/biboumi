#pragma once

#include <database/engine.hpp>

#include <database/select_query.hpp>
#include <database/row.hpp>

#include <algorithm>
#include <string>
#include <set>

using namespace std::string_literals;

template <typename T>
std::string ToSQLType(DatabaseEngine& db)
{
  if (std::is_same<T, Id>::value)
    return db.id_column_type();
  else if (std::is_same<typename T::real_type, std::string>::value)
    return "TEXT";
  else
    return "INTEGER";
}

template <typename ColumnType>
void add_column_to_table(DatabaseEngine& db, const std::string& table_name)
{
  const std::string name = ColumnType::name;
  std::string query{"ALTER TABLE " + table_name + " ADD " + ColumnType::name + " " + ToSQLType<ColumnType>(db)};
  auto res = db.raw_exec(query);
  if (std::get<0>(res) == false)
    log_error("Error adding column ", name, " to table ", table_name, ": ",  std::get<1>(res));
}

template <typename ColumnType, decltype(ColumnType::options) = nullptr>
void append_option(std::string& s)
{
  s += " "s + ColumnType::options;
}

template <typename, typename... Args>
void append_option(Args&& ...)
{ }

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

  void upgrade(DatabaseEngine& db)
  {
    const auto existing_columns = db.get_all_columns_from_table(this->name);
    add_column_if_not_exists(db, existing_columns);
  }

  void create(DatabaseEngine& db)
  {
    std::string query{"CREATE TABLE IF NOT EXISTS "};
    query += this->name;
    query += " (";
    this->add_column_create(db, query);
    query += ")";

    auto result = db.raw_exec(query);
    if (std::get<0>(result) == false)
      log_error("Error executing query: ", std::get<1>(result));
  }

  RowType row()
  {
    return {this->name};
  }

  auto select()
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
  add_column_if_not_exists(DatabaseEngine& db, const std::set<std::string>& existing_columns)
  {
    using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<ColumnTypes>()))>::type;
    if (existing_columns.count(ColumnType::name) == 0)
      add_column_to_table<ColumnType>(db, this->name);
    add_column_if_not_exists<N+1>(db, existing_columns);
  }
  template <std::size_t N=0>
  typename std::enable_if<N == sizeof...(T), void>::type
  add_column_if_not_exists(DatabaseEngine&, const std::set<std::string>&)
  {}

  template <std::size_t N=0>
  typename std::enable_if<N < sizeof...(T), void>::type
  add_column_create(DatabaseEngine& db, std::string& str)
  {
    using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<ColumnTypes>()))>::type;
    str += ColumnType::name;
    str += " ";
    str += ToSQLType<ColumnType>(db);
    if (N != sizeof...(T) - 1)
      str += ",";

    add_column_create<N+1>(db, str);
  }
  template <std::size_t N=0>
  typename std::enable_if<N == sizeof...(T), void>::type
  add_column_create(DatabaseEngine&, std::string&)
  { }

  const std::string name;
};
