#pragma once

#include <database/statement.hpp>
#include <database/query.hpp>
#include <logger/logger.hpp>
#include <database/row.hpp>

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
typename std::enable_if<std::is_same<std::string, T>::value, std::string>::type
extract_row_value(Statement& statement, const int i)
{
  const auto size = sqlite3_column_bytes(statement.get(), i);
  const unsigned char* str = sqlite3_column_text(statement.get(), i);
  std::string result(reinterpret_cast<const char*>(str), size);
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
      this->insert_col_name<0>();
      this->body += " from " + this->table_name;
    }

    template <std::size_t N>
    typename std::enable_if<N < sizeof...(T), void>::type
    insert_col_name()
    {
      using ColumnsType = std::tuple<T...>;
      ColumnsType tuple{};
      auto value = std::get<N>(tuple);

      this->body += " "s + value.name;

      if (N < (sizeof...(T) - 1))
        this->body += ", ";

      this->insert_col_name<N+1>();
    }
    template <std::size_t N>
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
      int i = 1;
      for (const std::string& param: this->params)
        {
          if (sqlite3_bind_text(statement.get(), i, param.data(), static_cast<int>(param.size()), SQLITE_TRANSIENT) != SQLITE_OK)
            log_debug("Failed to bind ", param, " to param ", i);
          else
            log_debug("Bound ", param, " to ", i);

          i++;
        }
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
