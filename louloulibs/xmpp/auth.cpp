#include <xmpp/auth.hpp>

#include <utils/sha1.hpp>

#include <iomanip>
#include <sstream>

std::string get_handshake_digest(const std::string& stream_id, const std::string& secret)
{
  sha1nfo sha1;
  sha1_init(&sha1);
  sha1_write(&sha1, stream_id.data(), stream_id.size());
  sha1_write(&sha1, secret.data(), secret.size());
  const uint8_t* result = sha1_result(&sha1);

  std::ostringstream digest;
  for (int i = 0; i < HASH_LENGTH; i++)
    digest << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(result[i]);

  return digest.str();
}
