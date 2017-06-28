#pragma once

#include <database/statement.hpp>
#include <database/query.hpp>
#include <logger/logger.hpp>
#include <database/row.hpp>

#include <utils/optional_bool.hpp>

#include <vector>
#include <string>

#include <sqlite3.h>

using namespace std::string_literals;

template <typename T>
typename std::enable_if<std::is_integral<T>::value, sqlite3_int64>::type
extract_row_value(Statement& statement, const int i)
{
  return sqlite3_column_int64(statement.get(), i);
}

template <typename T>
typename std::enable_if<std::is_same<std::string, T>::value, T>::type
extract_row_value(Statement& statement, const int i)
{
  const auto size = sqlite3_column_bytes(statement.get(), i);
  const unsigned char* str = sqlite3_column_text(statement.get(), i);
  std::string result(reinterpret_cast<const char*>(str), static_cast<std::size_t>(size));
  return result;
}

template <typename T>
typename std::enable_if<std::is_same<OptionalBool, T>::value, T>::type
extract_row_value(Statement& statement, const int i)
{
  const auto integer = sqlite3_column_int(statement.get(), i);
  OptionalBool result;
  if (integer > 0)
    result.set_value(true);
  else if (integer < 0)
    result.set_value(false);
  return result;
}

template <std::size_t N=0, typename... T>
typename std::enable_if<N < sizeof...(T), void>::type
extract_row_values(Row<T...>& row, Statement& statement)
{
  using ColumnType = typename std::remove_reference<decltype(std::get<N>(row.columns))>::type;

  auto&& column = std::get<N>(row.columns);
  column.value = static_cast<decltype(column.value)>(extract_row_value<typename ColumnType::real_type>(statement, N));

  extract_row_values<N+1>(row, statement);
}

template <std::size_t N=0, typename... T>
typename std::enable_if<N == sizeof...(T), void>::type
extract_row_values(Row<T...>&, Statement&)
{}

template <typename... T>
struct SelectQuery: public Query
{
    SelectQuery(std::string table_name):
        Query("SELECT"),
        table_name(table_name)
    {
      this->insert_col_name();
      this->body += " from " + this->table_name;
    }

    template <std::size_t N=0>
    typename std::enable_if<N < sizeof...(T), void>::type
    insert_col_name()
    {
      using ColumnsType = std::tuple<T...>;
      using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<ColumnsType>()))>::type;

      this->body += " "s + ColumnType::name;

      if (N < (sizeof...(T) - 1))
        this->body += ", ";

      this->insert_col_name<N+1>();
    }
    template <std::size_t N=0>
    typename std::enable_if<N == sizeof...(T), void>::type
    insert_col_name()
    {}

  SelectQuery& where()
    {
      this->body += " WHERE ";
      return *this;
    };

    SelectQuery& order_by()
    {
      this->body += " ORDER BY ";
      return *this;
    }

    SelectQuery& limit()
    {
      this->body += " LIMIT ";
      return *this;
    }

    auto execute(sqlite3* db)
    {
      auto statement = this->prepare(db);
      std::vector<Row<T...>> rows;
      while (sqlite3_step(statement.get()) == SQLITE_ROW)
        {
          Row<T...> row(this->table_name);
          extract_row_values(row, statement);
          rows.push_back(row);
        }
      return rows;
    }

    const std::string table_name;
};

