#pragma once

#include <database/engine.hpp>
#include <database/query.hpp>
#include <database/row.hpp>

using namespace std::string_literals;

template <class T, class... Tuple>
struct Index;

template <class T, class... Types>
struct Index<T, std::tuple<T, Types...>>
{
  static const std::size_t value = 0;
};

template <class T, class U, class... Types>
struct Index<T, std::tuple<U, Types...>>
{
  static const std::size_t value = Index<T, std::tuple<Types...>>::value + 1;
};

struct UpdateQuery: public Query
{
  template <typename... T>
  UpdateQuery(const std::string& name, const std::tuple<T...>& columns):
      Query("UPDATE ")
  {
    this->body += name;
    this->insert_col_names_and_values(columns);
  }

  template <typename... T>
  void insert_col_names_and_values(const std::tuple<T...>& columns)
  {
    this->body += " SET ";
    this->insert_col_name_and_value(columns);
    this->body += " WHERE "s + Id::name + "=$" + std::to_string(this->current_param);
  }

  template <int N=0, typename... T>
  typename std::enable_if<N < sizeof...(T), void>::type
  insert_col_name_and_value(const std::tuple<T...>& columns)
  {
    using ColumnType = std::decay_t<decltype(std::get<N>(columns))>;

    if (!std::is_same<ColumnType, Id>::value)
      {
        this->body += ColumnType::name + "=$"s + std::to_string(this->current_param);
        this->current_param++;

        if (N < (sizeof...(T) - 1))
          this->body += ", ";
      }

    this->insert_col_name_and_value<N+1>(columns);
  }
  template <int N=0, typename... T>
  typename std::enable_if<N == sizeof...(T), void>::type
  insert_col_name_and_value(const std::tuple<T...>&)
  {}


  template <typename... T>
  void execute(DatabaseEngine& db, const std::tuple<T...>& columns)
  {
#ifdef DEBUG_SQL_QUERIES
      const auto timer = this->log_and_time();
#endif

    auto statement = db.prepare(this->body);
    this->bind_param(columns, *statement);
    this->bind_id(columns, *statement);

    statement->step();
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
  void bind_id(const std::tuple<T...>& columns, Statement& statement)
  {
    static constexpr auto index = Index<Id, std::tuple<T...>>::value;
    auto&& value = std::get<index>(columns);

    actual_bind(statement, value.value, sizeof...(T));
  }
};

template <typename... T>
void update(Row<T...>& row, DatabaseEngine& db)
{
  UpdateQuery query(row.table_name, row.columns);

  query.execute(db, row.columns);
}
