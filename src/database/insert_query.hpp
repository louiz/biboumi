#pragma once

#include <database/column.hpp>
#include <database/query.hpp>
#include <logger/logger.hpp>

#include <type_traits>
#include <vector>
#include <string>
#include <tuple>

#include <sqlite3.h>

template <int N, typename ColumnType, typename... T>
typename std::enable_if<!std::is_same<std::decay_t<ColumnType>, Id>::value, void>::type
actual_bind(sqlite3_stmt* statement, std::vector<std::string>& params, const std::tuple<T...>&)
{
  const auto value = params.front();
  params.erase(params.begin());
  if (sqlite3_bind_text(statement, N + 1, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT) != SQLITE_OK)
    log_error("Failed to bind ", value, " to param ", N);
  else
    log_debug("Bound (not id) [", value, "] to ", N);
}

template <int N, typename ColumnType, typename... T>
typename std::enable_if<std::is_same<std::decay_t<ColumnType>, Id>::value, void>::type
actual_bind(sqlite3_stmt* statement, std::vector<std::string>&, const std::tuple<T...>& columns)
{
  auto&& column = std::get<Id>(columns);
  if (column.value != 0)
    {
      if (sqlite3_bind_int64(statement, N + 1, column.value) != SQLITE_OK)
        log_error("Failed to bind ", column.value, " to id.");
    }
  else if (sqlite3_bind_null(statement, N + 1) != SQLITE_OK)
    log_error("Failed to bind NULL to param ", N);
  else
    log_debug("Bound NULL to ", N);
}

struct InsertQuery: public Query
{
  InsertQuery(const std::string& name):
      Query("INSERT OR REPLACE INTO ")
  {
    this->body += name;
  }

  template <typename... T>
  void execute(const std::tuple<T...>& columns, sqlite3* db)
  {
    auto statement = this->prepare(db);
    {
      this->bind_param<0>(columns, statement);
      if (sqlite3_step(statement) != SQLITE_DONE)
        log_error("Failed to execute query: ", sqlite3_errmsg(db));
    }
  }

  template <int N, typename... T>
  typename std::enable_if<N < sizeof...(T), void>::type
  bind_param(const std::tuple<T...>& columns, sqlite3_stmt* statement)
  {
    using ColumnType = typename std::remove_reference<decltype(std::get<N>(columns))>::type;

    actual_bind<N, ColumnType>(statement, this->params, columns);
    this->bind_param<N+1>(columns, statement);
  }

  template <int N, typename... T>
  typename std::enable_if<N == sizeof...(T), void>::type
  bind_param(const std::tuple<T...>&, sqlite3_stmt*)
  {}

  template <typename... T>
  void insert_values(const std::tuple<T...>& columns)
  {
    this->body += "VALUES (";
    this->insert_value<0>(columns);
    this->body += ")";
  }

  template <int N, typename... T>
  typename std::enable_if<N < sizeof...(T), void>::type
  insert_value(const std::tuple<T...>& columns)
  {
    this->body += "?";
    if (N != sizeof...(T) - 1)
      this->body += ",";
    this->body += " ";
    add_param(*this, std::get<N>(columns));
    this->insert_value<N+1>(columns);
  }
  template <int N, typename... T>
  typename std::enable_if<N == sizeof...(T), void>::type
  insert_value(const std::tuple<T...>&)
  { }

  template <typename... T>
  void insert_col_names(const std::tuple<T...>& columns)
  {
    this->body += " (";
    this->insert_col_name<0>(columns);
    this->body += ")\n";
  }

  template <int N, typename... T>
  typename std::enable_if<N < sizeof...(T), void>::type
  insert_col_name(const std::tuple<T...>& columns)
  {
    auto value = std::get<N>(columns);

    this->body += value.name;

    if (N < (sizeof...(T) - 1))
      this->body += ", ";

    this->insert_col_name<N+1>(columns);
  }
  template <int N, typename... T>
  typename std::enable_if<N == sizeof...(T), void>::type
  insert_col_name(const std::tuple<T...>&)
  {}


 private:
};
