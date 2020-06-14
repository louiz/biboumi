#include <utils/base64.hpp>

#ifdef BOTAN_FOUND
#include <botan/base64.h>
#endif

namespace base64
{

std::string encode(const std::string &input)
{
#ifdef BOTAN_FOUND
  return Botan::base64_encode(reinterpret_cast<const uint8_t*>(input.data()), input.size());
#else
#error "base64::encode() not yet implemented without Botan."
#endif
}

}
