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
auto extract_row_value(Statement& statement, const int i)
{
  if constexpr(std::is_integral<T>::value)
    return statement.get_column_int64(i);
  else if constexpr (std::is_same<std::string, T>::value)
    return statement.get_column_text(i);
  else if (std::is_same<std::optional<bool>, T>::value)
    {
      const auto integer = statement.get_column_int(i);
      if (integer > 0)
        return std::optional<bool>{true};
      else if (integer < 0)
        return std::optional<bool>{false};
      return std::optional<bool>{};
    }
}

template <std::size_t N=0, typename... T>
void extract_row_values(Row<T...>& row, Statement& statement)
{
  if constexpr(N < sizeof...(T))
    {
      using ColumnType = typename std::remove_reference<decltype(std::get<N>(row.columns))>::type;

      auto&& column = std::get<N>(row.columns);
      column.value = static_cast<decltype(column.value)>(extract_row_value<typename ColumnType::real_type>(statement, N));

      extract_row_values<N + 1>(row, statement);
    }
}

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
    void insert_col_name()
    {
      if constexpr(N < sizeof...(T))
        {
          using ColumnsType = std::tuple<T...>;
          using ColumnType = typename std::remove_reference<decltype(std::get<N>(std::declval<ColumnsType>()))>::type;

          this->body += " " + std::string{ColumnType::name};

          if (N < (sizeof...(T) - 1))
            this->body += ", ";

          this->insert_col_name<N + 1>();
        }
    }

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

