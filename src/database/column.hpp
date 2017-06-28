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

struct Id: Column<std::size_t> { static constexpr auto name = "id_";
                                 static constexpr auto options = "PRIMARY KEY AUTOINCREMENT"; };
