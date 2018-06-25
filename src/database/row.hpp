#pragma once

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

  void clear()
  {
    this->clear_col<0>();
  }

  std::tuple<T...> columns{};
  std::string table_name;

private:
  template <std::size_t N>
  typename std::enable_if<N < sizeof...(T), void>::type
  clear_col()
  {
    std::get<N>(this->columns).clear();
    this->clear_col<N+1>();
  }
  template <std::size_t N>
  typename std::enable_if<N == sizeof...(T), void>::type
  clear_col()
  { }
};
