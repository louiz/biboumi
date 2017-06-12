#pragma once

#include <cstdint>

template <typename T>
struct Column
{
    using real_type = T;
    T value;
};

struct Id: Column<std::size_t> { static constexpr auto name = "id_";
                                 static constexpr auto options = "PRIMARY KEY AUTOINCREMENT"; };
