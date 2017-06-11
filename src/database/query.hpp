#pragma once

#include <sqlite3.h>

#include <iostream>
#include <vector>
#include <string>

struct Query
{
    std::string body;
    std::vector<std::string> params;

    Query(std::string str):
        body(std::move(str))
    {}

    sqlite3_stmt* prepare(sqlite3* db)
    {
      sqlite3_stmt* statement;
      auto res = sqlite3_prepare(db, this->body.data(), this->body.size() + 1,
                                 &statement, nullptr);
      if (res != SQLITE_OK)
        {
          std::cout << "Error preparing statement: " << sqlite3_errmsg(db) << std::endl;
          return nullptr;
        }
      return statement;
    }

};


template <typename ColumnType>
void add_param(Query& query, const ColumnType& column)
{
  actual_add_param(query, column.value);
}

template <>
void add_param<Id>(Query&, const Id&)
{}

template <typename T>
void actual_add_param(Query& query, const T& val)
{
  query.params.push_back(std::to_string(val));
}

void actual_add_param(Query& query, const std::string& val)
{
  query.params.push_back(val);
}
