#pragma once

#include <utils/optional_bool.hpp>
#include <database/statement.hpp>
#include <database/column.hpp>

#include <logger/logger.hpp>

#include <vector>
#include <string>

void actual_bind(Statement& statement, const std::string& value, int index);
void actual_bind(Statement& statement, const std::size_t value, int index);
void actual_bind(Statement& statement, const OptionalBool& value, int index);

struct Query
{
    std::string body;
    std::vector<std::string> params;
    int current_param{1};

    Query(std::string str):
        body(std::move(str))
    {}
};

template <typename ColumnType>
void add_param(Query& query, const ColumnType& column)
{
  std::cout << "add_param<ColumnType>" << std::endl;
  actual_add_param(query, column.value);
}

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
  query.body += "$" + std::to_string(query.current_param++);
  actual_add_param(query, i);
  return query;
}
