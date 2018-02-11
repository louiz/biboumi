#pragma once

#include <cstddef>

template <typename T>
struct Column
{
    Column(T default_value):
        value{default_value} {}
    Column():
        value{} {}
    using real_type = T;
    T value{};
};

struct Id: Column<std::size_t> {
    static constexpr std::size_t unset_value = static_cast<std::size_t>(-1);
    static constexpr auto name = "id_";
    static constexpr auto options = "PRIMARY KEY";

    Id(): Column<std::size_t>(unset_value) {}
};
