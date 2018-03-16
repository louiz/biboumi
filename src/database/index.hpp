#pragma once

#include <database/engine.hpp>

#include <string>
#include <tuple>

namespace
{
template <std::size_t N=0, typename... T>
void add_column_name(std::string& out)
{
  if constexpr(N < sizeof...(T))
    {
      using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<std::tuple<T...>>()))>::type;
      out += ColumnType::name;
      if (N != sizeof...(T) - 1)
        out += ",";
      add_column_name<N + 1, T...>(out);
    }
}
}

template <typename... Columns>
void create_index(DatabaseEngine& db, const std::string& name, const std::string& table)
{
  std::string query{"CREATE INDEX IF NOT EXISTS "};
  query += name + " ON " + table + "(";
  add_column_name<0, Columns...>(query);
  query += ")";

  auto result = db.raw_exec(query);
  if (std::get<0>(result) == false)
    log_error("Error executing query: ", std::get<1>(result));
}
