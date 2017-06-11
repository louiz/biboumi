#pragma once

#include <database/query.hpp>
#include <database/row.hpp>

#include <iostream>
#include <vector>
#include <string>

#include <sqlite3.h>

using namespace std::string_literals;

template <typename T>
typename std::enable_if<std::is_integral<T>::value, sqlite3_int64>::type
extract_row_value(sqlite3_stmt* statement, const std::size_t i)
{
  return sqlite3_column_int64(statement, i);
}

template <typename T>
typename std::enable_if<std::is_same<std::string, T>::value, std::string>::type
extract_row_value(sqlite3_stmt* statement, const std::size_t i)
{
  const auto size = sqlite3_column_bytes(statement, i);
  const unsigned char* str = sqlite3_column_text(statement, i);
  std::string result(reinterpret_cast<const char*>(str), size);
  return result;
}

template <std::size_t N=0, typename... T>
typename std::enable_if<N < sizeof...(T), void>::type
extract_row_values(Row<T...>& row, sqlite3_stmt* statement)
{
  using ColumnType = typename std::remove_reference<decltype(std::get<N>(row.columns))>::type;

  auto&& column = std::get<N>(row.columns);
  column.value = extract_row_value<typename ColumnType::real_type>(statement, N);

  extract_row_values<N+1>(row, statement);
}

template <std::size_t N=0, typename... T>
typename std::enable_if<N == sizeof...(T), void>::type
extract_row_values(Row<T...>&, sqlite3_stmt*)
{}

template <typename... T>
struct SelectQuery: public Query
{
    SelectQuery(std::string table_name):
        Query("SELECT * from "s + table_name),
        table_name(table_name)
    {}

    SelectQuery& where()
    {
      this->body += " WHERE ";
      return *this;
    };

    auto execute(sqlite3* db)
    {
      std::cout << "Execute, size of T..." << sizeof...(T) << std::endl;
      auto statement = this->prepare(db);
      int i = 1;
      for (const std::string& param: this->params)
        {
          if (sqlite3_bind_text(statement, i, param.data(), param.size(), SQLITE_TRANSIENT) != SQLITE_OK)
            std::cout << "Failed to bind " << param << " to param " << i << std::endl;
          else
            std::cout << "Bound " << param << " to " << i << std::endl;

          i++;
        }
      std::vector<Row<T...>> rows;
      while (sqlite3_step(statement) == SQLITE_ROW)
        {
          std::cout << "result." << std::endl;
          Row<T...> row;
          extract_row_values(row, statement);
          rows.push_back(row);
        }
      return rows;
    }

    const std::string table_name;
};

template <typename T, typename... ColumnTypes>
typename std::enable_if<!std::is_integral<T>::value, SelectQuery<ColumnTypes...>&>::type
operator<<(SelectQuery<ColumnTypes...>& query, const T&)
{
  query.body += T::name;
  return query;
}

template <typename... ColumnTypes>
SelectQuery<ColumnTypes...>& operator<<(SelectQuery<ColumnTypes...>& query, const char* str)
{
  query.body += str;
  return query;
}

template <typename... ColumnTypes>
SelectQuery<ColumnTypes...>& operator<<(SelectQuery<ColumnTypes...>& query, const std::string& str)
{
  query.body += "?";
  actual_add_param(query, str);
  return query;
}

template <typename Integer, typename... ColumnTypes>
typename std::enable_if<std::is_integral<Integer>::value, SelectQuery<ColumnTypes...>&>::type
operator<<(SelectQuery<ColumnTypes...>& query, const Integer& i)
{
  query.body += "?";
  actual_add_param(query, i);
  return query;
}
