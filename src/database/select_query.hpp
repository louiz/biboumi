#pragma once

#include <database/engine.hpp>

#include <database/table.hpp>
#include <database/database.hpp>
#include <database/statement.hpp>
#include <utils/datetime.hpp>
#include <database/query.hpp>
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

template <typename T>
typename std::enable_if<std::is_same<DateTime, T>::value, T>::type
extract_row_value(Statement& statement, const int i)
{
  const std::string timestamp = statement.get_column_text(i);
  return {timestamp};
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

template <typename ColumnType>
std::string before_column()
{
  return {};
}

template <typename ColumnType>
std::string after_column()
{
  return {};
}

template <>
std::string before_column<Database::Date>();

template <>
std::string after_column<Database::Date>();

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

      this->body += " ";
      this->body += before_column<ColumnType>() + ColumnType::name + after_column<ColumnType>();

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
      if (!statement)
        return rows;
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

template <typename... T>
auto select(const Table<T...> table)
{
  SelectQuery<T...> query(table.name);
  return query;
}
