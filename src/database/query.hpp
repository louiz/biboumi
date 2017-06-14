#pragma once

#include <database/statement.hpp>

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
      sqlite3_stmt* statement;
      log_debug(this->body);
      auto res = sqlite3_prepare(db, this->body.data(), static_cast<int>(this->body.size()) + 1,
                                 &statement, nullptr);
      if (res != SQLITE_OK)
        {
          log_error("Error preparing statement: ", sqlite3_errmsg(db));
          return nullptr;
        }
      return {statement};
    }
};

template <typename ColumnType>
void add_param(Query& query, const ColumnType& column)
{
  actual_add_param(query, column.value);
}

template <typename T>
void actual_add_param(Query& query, const T& val)
{
  query.params.push_back(std::to_string(val));
}

void actual_add_param(Query& query, const std::string& val);
