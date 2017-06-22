#include <database/query.hpp>
#include <database/column.hpp>

template <>
void add_param<Id>(Query&, const Id&)
{}

void actual_add_param(Query& query, const std::string& val)
{
  query.params.push_back(val);
}

void actual_add_param(Query& query, const OptionalBool& val)
{
  if (!val.is_set)
    query.params.push_back("0");
  else if (val.value)
    query.params.push_back("1");
  else
    query.params.push_back("-1");
}

Query& operator<<(Query& query, const char* str)
{
  query.body += str;
  return query;
}

Query& operator<<(Query& query, const std::string& str)
{
  query.body += "?";
  actual_add_param(query, str);
  return query;
}
