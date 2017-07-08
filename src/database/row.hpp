#pragma once

#include <database/insert_query.hpp>
#include <logger/logger.hpp>

#include <type_traits>

#include <sqlite3.h>

template <typename ColumnType, typename... T>
typename std::enable_if<!std::is_same<std::decay_t<ColumnType>, Id>::value, void>::type
update_id(std::tuple<T...>&, sqlite3*)
{}

template <typename ColumnType, typename... T>
typename std::enable_if<std::is_same<std::decay_t<ColumnType>, Id>::value, void>::type
update_id(std::tuple<T...>& columns, sqlite3* db)
{
  auto&& column = std::get<ColumnType>(columns);
  log_debug("Found an autoincrement col.");
  auto res = sqlite3_last_insert_rowid(db);
  log_debug("Value is now: ", res);
  column.value = static_cast<Id::real_type>(res);
}

template <std::size_t N=0, typename... T>
typename std::enable_if<N < sizeof...(T), void>::type
update_autoincrement_id(std::tuple<T...>& columns, sqlite3* db)
{
  using ColumnType = typename std::remove_reference<decltype(std::get<N>(columns))>::type;
  update_id<ColumnType>(columns, db);
  update_autoincrement_id<N+1>(columns, db);
}

template <std::size_t N=0, typename... T>
typename std::enable_if<N == sizeof...(T), void>::type
update_autoincrement_id(std::tuple<T...>&, sqlite3*)
{}

template <typename... T>
struct Row
{
    Row(std::string name):
    table_name(std::move(name))
    {}

    template <typename Type>
    typename Type::real_type& col()
    {
      auto&& col = std::get<Type>(this->columns);
      return col.value;
    }

    template <typename Type>
    const auto& col() const
    {
      auto&& col = std::get<Type>(this->columns);
      return col.value;
    }

    void save(sqlite3* db)
    {
      InsertQuery query(this->table_name);
      query.insert_col_names(this->columns);
      query.insert_values(this->columns);
      log_debug(query.body);

      query.execute(this->columns, db);

      update_autoincrement_id(this->columns, db);
    }

    std::tuple<T...> columns;
    std::string table_name;
};
