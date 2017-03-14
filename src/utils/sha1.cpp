#include <utils/sha1.hpp>

#include <biboumi.h>

#ifdef BOTAN_FOUND
# include <botan/version.h>
# include <botan/hash.h>
# include <botan/hex.h>
# include <botan/exceptn.h>
#endif
#ifdef GCRYPT_FOUND
# include <gcrypt.h>
# include <vector>
# include <iomanip>
# include <sstream>
#endif

std::string sha1(const std::string& input)
{
#ifdef BOTAN_FOUND
# if BOTAN_VERSION_CODE < BOTAN_VERSION_CODE_FOR(1,11,34)
  auto sha1 = Botan::HashFunction::create("SHA-1");
  if (!sha1)
    throw Botan::Algorithm_Not_Found("SHA-1");
# else
  auto sha1 = Botan::HashFunction::create_or_throw("SHA-1");
# endif
  sha1->update(input);
  return Botan::hex_encode(sha1->final(), false);
#endif
#ifdef GCRYPT_FOUND
  const auto hash_length = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
  std::vector<uint8_t> output(hash_length, {});
  gcry_md_hash_buffer(GCRY_MD_SHA1, output.data(), input.data(), input.size());
  std::ostringstream digest;
  for (std::size_t i = 0; i < hash_length; i++)
    digest << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(output[i]);
  return digest.str();
#endif
}
