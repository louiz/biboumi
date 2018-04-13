#pragma once

#include <utils/is_one_of.hpp>

#include <type_traits>

template <typename... T>
struct Row
{
  Row(std::string name):
      table_name(std::move(name))
  {}

  template <typename Type>
  typename Type::real_type& col()
  {
    auto&& col = std::get<Type>(this->columns);
    return col.value;
  }

  template <typename Type>
  const auto& col() const
  {
    auto&& col = std::get<Type>(this->columns);
    return col.value;
  }

public:
  std::tuple<T...> columns;
  std::string table_name;
};
