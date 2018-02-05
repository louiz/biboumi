#pragma once

#include <database/engine.hpp>

#include <database/statement.hpp>
#include <database/query.hpp>
#include <logger/logger.hpp>
#include <database/row.hpp>

#include <utils/optional_bool.hpp>

#include <vector>
#include <string>

using namespace std::string_literals;

template <typename T>
typename std::enable_if<std::is_integral<T>::value, std::int64_t>::type
extract_row_value(Statement& statement, const int i)
{
  return statement.get_column_int64(i);
}

template <typename T>
typename std::enable_if<std::is_same<std::string, T>::value, T>::type
extract_row_value(Statement& statement, const int i)
{
  return statement.get_column_text(i);
}

template <typename T>
typename std::enable_if<std::is_same<OptionalBool, T>::value, T>::type
extract_row_value(Statement& statement, const int i)
{
  const auto integer = statement.get_column_int(i);
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

      this->body += " " + std::string{ColumnType::name};

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

    auto execute(DatabaseEngine& db)
    {
      std::vector<Row<T...>> rows;

#ifdef DEBUG_SQL_QUERIES
      const auto timer = this->log_and_time();
#endif

      auto statement = db.prepare(this->body);
      statement->bind(std::move(this->params));

      while (statement->step() == StepResult::Row)
        {
          Row<T...> row(this->table_name);
          extract_row_values(row, *statement);
          rows.push_back(row);
        }

      return rows;
    }

    const std::string table_name;
};

