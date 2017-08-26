#pragma once

#include <sqlite3.h>

#include <string>
#include <tuple>

namespace
{
template <std::size_t N=0, typename... T>
typename std::enable_if<N == sizeof...(T), void>::type
add_column_name(std::string&)
{ }

template <std::size_t N=0, typename... T>
typename std::enable_if<N < sizeof...(T), void>::type
add_column_name(std::string& out)
{
  using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<std::tuple<T...>>()))>::type;
  out += ColumnType::name;
  if (N != sizeof...(T) - 1)
    out += ",";
  add_column_name<N+1, T...>(out);
}
}

template <typename... Columns>
void create_index(sqlite3* db, const std::string& name, const std::string& table)
{
  std::string res{"CREATE INDEX IF NOT EXISTS "};
  res += name + " ON " + table + "(";
  add_column_name<0, Columns...>(res);
  res += ")";

  char* error;
  const auto result = sqlite3_exec(db, res.data(), nullptr, nullptr, &error);
  if (result != SQLITE_OK)
    {
      log_error("Error executing query: ", error);
      sqlite3_free(error);
    }
}
