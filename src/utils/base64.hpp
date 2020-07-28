#pragma once

#include "biboumi.h"

#ifdef BOTAN_FOUND

#include <string>

namespace base64
{
std::string encode(const std::string& input);
}

#endif
