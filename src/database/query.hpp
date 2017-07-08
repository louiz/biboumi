#pragma once

#include <utils/optional_bool.hpp>
#include <database/statement.hpp>
#include <database/column.hpp>

#include <logger/logger.hpp>

#include <vector>
#include <string>

#include <sqlite3.h>

struct Query
{
    std::string body;
    std::vector<std::string> params;

    Query(std::string str):
        body(std::move(str))
    {}

    Statement prepare(sqlite3* db)
    {
      sqlite3_stmt* stmt;
      auto res = sqlite3_prepare(db, this->body.data(), static_cast<int>(this->body.size()) + 1,
                                 &stmt, nullptr);
      if (res != SQLITE_OK)
        {
          log_error("Error preparing statement: ", sqlite3_errmsg(db));
          return nullptr;
        }
      Statement statement(stmt);
      int i = 1;
      for (const std::string& param: this->params)
        {
          if (sqlite3_bind_text(statement.get(), i, param.data(), static_cast<int>(param.size()), SQLITE_TRANSIENT) != SQLITE_OK)
            log_error("Failed to bind ", param, " to param ", i);
          i++;
        }

      return statement;
    }

    void execute(sqlite3* db)
    {
      auto statement = this->prepare(db);
      while (sqlite3_step(statement.get()) != SQLITE_DONE)
        ;
    }
};

template <typename ColumnType>
void add_param(Query& query, const ColumnType& column)
{
  actual_add_param(query, column.value);
}
template <>
void add_param<Id>(Query& query, const Id& column);

template <typename T>
void actual_add_param(Query& query, const T& val)
{
  query.params.push_back(std::to_string(val));
}

void actual_add_param(Query& query, const std::string& val);
void actual_add_param(Query& query, const OptionalBool& val);

template <typename T>
typename std::enable_if<!std::is_integral<T>::value, Query&>::type
operator<<(Query& query, const T&)
{
  query.body += T::name;
  return query;
}

Query& operator<<(Query& query, const char* str);
Query& operator<<(Query& query, const std::string& str);
template <typename Integer>
typename std::enable_if<std::is_integral<Integer>::value, Query&>::type
operator<<(Query& query, const Integer& i)
{
  query.body += "?";
  actual_add_param(query, i);
  return query;
}
