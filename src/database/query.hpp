#pragma once

#include <biboumi.h>

#include <utils/optional_bool.hpp>
#include <database/statement.hpp>
#include <database/column.hpp>

#include <logger/logger.hpp>

#include <vector>
#include <string>

void actual_bind(Statement& statement, const std::string& value, int index);
void actual_bind(Statement& statement, const OptionalBool& value, int index);
template <typename T>
void actual_bind(Statement& statement, const T& value, int index)
{
  static_assert(std::is_integral<T>::value,
                "Only a string, an optional-bool or an integer can be used.");
  statement.bind_int64(index, static_cast<std::int64_t>(value));
}

#ifdef DEBUG_SQL_QUERIES
#include <utils/scopetimer.hpp>

inline auto make_sql_timer()
{
  return make_scope_timer([](const std::chrono::steady_clock::duration& elapsed)
                          {
                            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed);
                            const auto rest = elapsed - seconds;
                            log_debug("Query executed in ", seconds.count(), ".", rest.count(), "s.");
                          });
}
#endif

struct Query
{
    std::string body;
    std::vector<std::string> params;
    int current_param{1};

    Query(std::string str):
        body(std::move(str))
    {}

#ifdef DEBUG_SQL_QUERIES
    auto log_and_time()
    {
       std::ostringstream os;
       os << this->body << "; ";
       for (const auto& param: this->params)
         os << "'" << param << "' ";
       log_debug("SQL QUERY: ", os.str());
       return make_sql_timer();
    }
#endif
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

