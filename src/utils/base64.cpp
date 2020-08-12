#include <utils/base64.hpp>

#ifdef BOTAN_FOUND
#include <botan/base64.h>

namespace base64
{

std::string encode(const std::string &input)
{
  return Botan::base64_encode(reinterpret_cast<const uint8_t*>(input.data()), input.size());
}

}

#endif
