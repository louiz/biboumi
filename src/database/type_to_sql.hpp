#pragma once

#include <string>

template <typename T>
struct TypeToSQLType { static const std::string type; };

template <> const std::string TypeToSQLType<int>::type;
template <> const std::string TypeToSQLType<std::size_t>::type;
template <> const std::string TypeToSQLType<long>::type;
template <> const std::string TypeToSQLType<long long>::type;
template <> const std::string TypeToSQLType<bool>::type;
template <> const std::string TypeToSQLType<std::string>::type;