#include "biboumi.h"

#ifdef BOTAN_FOUND

#include <fstream>

#include <utils/tolower.hpp>

#include <network/tls_policy.hpp>
#include <logger/logger.hpp>

bool BiboumiTLSPolicy::load(const std::string& filename)
{
  std::ifstream is(filename.data());
  if (is)
    {
      try {
          this->load(is);
          log_info("Successfully loaded policy file: ", filename);
          return true;
        } catch (const Botan::Exception& e) {
          log_error("Failed to parse policy_file ", filename, ": ", e.what());
          return false;
        }
    }
  log_info("Could not open policy file: ", filename);
  return false;
}

void BiboumiTLSPolicy::load(std::istream& is)
{
  const auto dict = Botan::read_cfg(is);
  for (const auto& pair: dict)
    {
      // Workaround for options that are not overridden in Botan::TLS::Text_Policy
      if (pair.first == "require_cert_revocation_info")
        this->req_cert_revocation_info = !(pair.second == "0" || utils::tolower(pair.second) == "false");
      else
        this->set(pair.first, pair.second);
    }
}

bool BiboumiTLSPolicy::require_cert_revocation_info() const
{
  return this->req_cert_revocation_info;
}

#endif
