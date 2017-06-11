#pragma once

#include <database/insert_query.hpp>

#include <sqlite3.h>

#include <type_traits>

#include <iostream>

template <typename ColumnType, typename... T>
typename std::enable_if<!std::is_same<std::decay_t<ColumnType>, Id>::value, void>::type
update_id(std::tuple<T...>&, sqlite3*)
{};

template <typename ColumnType, typename... T>
typename std::enable_if<std::is_same<std::decay_t<ColumnType>, Id>::value, void>::type
update_id(std::tuple<T...>& columns, sqlite3* db)
{
  auto&& column = std::get<ColumnType>(columns);
  std::cout << "Found an autoincrement col." << std::endl;
  auto res = sqlite3_last_insert_rowid(db);
  std::cout << "Value is now: " << res << std::endl;
  column.value = res;
};

template <std::size_t N, typename... T>
typename std::enable_if<N < sizeof...(T), void>::type
update_autoincrement_id(std::tuple<T...>& columns, sqlite3* db)
{
  using ColumnType = typename std::remove_reference<decltype(std::get<N>(columns))>::type;
  update_id<ColumnType>(columns, db);
  update_autoincrement_id<N+1>(columns, db);
};

template <std::size_t N, typename... T>
typename std::enable_if<N == sizeof...(T), void>::type
update_autoincrement_id(std::tuple<T...>&, sqlite3*)
{};

template <typename... T>
struct Row
{
    Row(std::string name):
    table_name(std::move(name))
    {}
    Row() = default;

    template <typename Type>
    auto& col()
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
      query.insert_values(this->columns);
      std::cout << query.body << std::endl;

      query.execute(this->columns, db);

      std::cout << "trying to update the row with the inserted ID" << std::endl;
      update_autoincrement_id<0>(this->columns, db);
    }

    std::tuple<T...> columns;
    std::string table_name;
};
