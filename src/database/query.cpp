#include <database/query.hpp>
#include <database/column.hpp>

template <>
void add_param<Id>(Query&, const Id&)
{}

void actual_add_param(Query& query, const std::string& val)
{
  query.params.push_back(val);
}
