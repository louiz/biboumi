#pragma once

#include <database/statement.hpp>
#include <database/column.hpp>
#include <database/query.hpp>
#include <logger/logger.hpp>

#include <type_traits>
#include <vector>
#include <string>
#include <tuple>

#include <sqlite3.h>

template <std::size_t N=0, typename... T>
typename std::enable_if<N < sizeof...(T), void>::type
update_autoincrement_id(std::tuple<T...>& columns, Statement& statement)
{
  using ColumnType = typename std::decay<decltype(std::get<N>(columns))>::type;
  if (std::is_same<ColumnType, Id>::value)
    {
      log_debug("EXTRACTING LAST ID");
      auto&& column = std::get<Id>(columns);
    }
  update_autoincrement_id<N+1>(columns, statement);
}

template <std::size_t N=0, typename... T>
typename std::enable_if<N == sizeof...(T), void>::type
update_autoincrement_id(std::tuple<T...>&, Statement& statement)
{}

struct InsertQuery: public Query
{
  template <typename... T>
  InsertQuery(const std::string& name, const std::tuple<T...>& columns):
      Query("INSERT INTO ")
  {
    this->body += name;
    this->insert_col_names(columns);
    this->insert_values(columns);
  }

  template <typename... T>
  void execute(DatabaseEngine& db, std::tuple<T...>& columns)
  {
    auto statement = db.prepare(this->body);
    this->bind_param(columns, *statement);

    if (statement->step() != StepResult::Error)
      db.extract_last_insert_rowid(*statement);
    else
      log_error("Failed to extract the rowid from the last INSERT");
  }

  template <int N=0, typename... T>
  typename std::enable_if<N < sizeof...(T), void>::type
  bind_param(const std::tuple<T...>& columns, Statement& statement, int index=1)
  {
    auto&& column = std::get<N>(columns);
    using ColumnType = std::decay_t<decltype(column)>;

    if (!std::is_same<ColumnType, Id>::value)
      actual_bind(statement, column.value, index++);

    this->bind_param<N+1>(columns, statement, index);
  }

  template <int N=0, typename... T>
  typename std::enable_if<N == sizeof...(T), void>::type
  bind_param(const std::tuple<T...>&, Statement&, int)
  {}

  template <typename... T>
  void insert_values(const std::tuple<T...>& columns)
  {
    this->body += "VALUES (";
    this->insert_value(columns);
    this->body += ")";
  }

  template <int N=0, typename... T>
  typename std::enable_if<N < sizeof...(T), void>::type
  insert_value(const std::tuple<T...>& columns, int index=1)
  {
    using ColumnType = std::decay_t<decltype(std::get<N>(columns))>;

    if (!std::is_same<ColumnType, Id>::value)
      {
        this->body += "$" + std::to_string(index++);
        if (N != sizeof...(T) - 1)
          this->body += ", ";
      }
    this->insert_value<N+1>(columns, index);
  }
  template <int N=0, typename... T>
  typename std::enable_if<N == sizeof...(T), void>::type
  insert_value(const std::tuple<T...>&, const int)
  { }

  template <typename... T>
  void insert_col_names(const std::tuple<T...>& columns)
  {
    this->body += " (";
    this->insert_col_name(columns);
    this->body += ")";
  }

  template <int N=0, typename... T>
  typename std::enable_if<N < sizeof...(T), void>::type
  insert_col_name(const std::tuple<T...>& columns)
  {
    using ColumnType = std::decay_t<decltype(std::get<N>(columns))>;

    if (!std::is_same<ColumnType, Id>::value)
      {
        this->body += ColumnType::name;

        if (N < (sizeof...(T) - 1))
          this->body += ", ";
      }

    this->insert_col_name<N+1>(columns);
  }

  template <int N=0, typename... T>
  typename std::enable_if<N == sizeof...(T), void>::type
  insert_col_name(const std::tuple<T...>&)
  {}
};
