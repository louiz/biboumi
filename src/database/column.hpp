#pragma once

#include <cstddef>

template <typename T>
struct Column
{
    Column(T default_value):
        value{default_value} {}
    Column():
        value{} {}
    void clear()
    {
      this->value = {};
    }
    using real_type = T;
    T value{};
};

template <typename T>
struct UnclearableColumn: public Column<T>
{
    using Column<T>::Column;
    void clear()
    { }
};

struct ForeignKey: Column<std::size_t> {
    static constexpr auto name = "fk_";
};

struct Id: UnclearableColumn<std::size_t> {
    static constexpr std::size_t unset_value = static_cast<std::size_t>(-1);
    static constexpr auto name = "id_";
    static constexpr auto options = "PRIMARY KEY";

    Id(): UnclearableColumn<std::size_t>(unset_value) {}
};
