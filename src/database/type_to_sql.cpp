#include <database/type_to_sql.hpp>

template <> const std::string TypeToSQLType<int>::type = "INTEGER";
template <> const std::string TypeToSQLType<std::size_t>::type = "INTEGER";
template <> const std::string TypeToSQLType<std::string>::type = "TEXT";
