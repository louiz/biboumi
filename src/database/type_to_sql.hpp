#pragma once

template <typename T>
struct TypeToSQLType { static const std::string type; };

template <> const std::string TypeToSQLType<int>::type = "INTEGER";
template <> const std::string TypeToSQLType<std::size_t>::type = "INTEGER";
template <> const std::string TypeToSQLType<std::string>::type = "TEXT";
