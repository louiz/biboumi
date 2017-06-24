#include <xmpp/auth.hpp>

#include <utils/sha1.hpp>

std::string get_handshake_digest(const std::string& stream_id, const std::string& secret)
{
  return sha1(stream_id + secret);
}
