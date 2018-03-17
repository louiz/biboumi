#pragma once

#include <biboumi.h>

#include <utils/optional_bool.hpp>
#include <database/statement.hpp>
#include <database/column.hpp>

#include <logger/logger.hpp>

#include <vector>
#include <string>

void actual_bind(Statement& statement, const std::string& value, int index);
void actual_bind(Statement& statement, const std::int64_t& value, int index);
void actual_bind(Statement& statement, const std::optional<bool>& value, int index);
template <typename T>
void actual_bind(Statement& statement, const T& value, int index)
{
  actual_bind(statement, static_cast<std::int64_t>(value), index);
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

void actual_add_param(Query& query, const std::string& val);
void actual_add_param(Query& query, const std::optional<bool>& val);
template <typename T>
void actual_add_param(Query& query, const T& val)
{
  query.params.push_back(std::to_string(val));
}

Query& operator<<(Query& query, const char* str);
Query& operator<<(Query& query, const std::string& str);
template <typename T>
Query& operator<<(Query& query, const T& i)
{
  if constexpr(std::is_integral<T>::value)
    {
      query.body += "$" + std::to_string(query.current_param++);
      actual_add_param(query, i);
    }
  else
    {
      query.body += T::name;
    }
  return query;
}
