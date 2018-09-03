#include <utils/uuid.hpp>
#include <uuid/uuid.h>

namespace utils
{
std::string gen_uuid()
{
  char uuid_str[37];
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_str);
  return uuid_str;
}
}
