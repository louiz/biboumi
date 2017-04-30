#include "catch.hpp"
#include <network/tls_policy.hpp>

#ifdef BOTAN_FOUND
TEST_CASE("tls_policy")
{
  BiboumiTLSPolicy policy;
  const auto default_minimum_signature_strength = policy.minimum_signature_strength();
  const auto default_session_ticket_lifetime = policy.session_ticket_lifetime();
  const auto default_minimum_rsa_bits = policy.minimum_rsa_bits();

  policy.load("does not exist");
  WHEN("we fail to load the file")
    {
      THEN("all values are the default ones")
        {
          CHECK(policy.minimum_signature_strength() == default_minimum_signature_strength);
          CHECK(policy.minimum_rsa_bits() == default_minimum_rsa_bits);
        }
      AND_WHEN("we load a valid first file")
        {
          std::istringstream iss("minimum_signature_strength  =      128\nminimum_rsa_bits=12\n");
          policy.load(iss);
          THEN("the specified values are updated, and the rest is still the default")
            {
              CHECK(policy.minimum_signature_strength() == 128);
              CHECK(policy.minimum_rsa_bits() == 12);
              CHECK(policy.session_ticket_lifetime() == default_session_ticket_lifetime);
            }
          AND_WHEN("we load a second file")
            {
              std::istringstream iss("minimum_signature_strength  =      15");
              policy.load(iss);
              THEN("the specified values are updated, and the rest is untouched")
                {
                  CHECK(policy.minimum_signature_strength() == 15);
                  CHECK(policy.minimum_rsa_bits() == 12);
                  CHECK(policy.session_ticket_lifetime() == default_session_ticket_lifetime);
                }
            }
        }
    }
}
#endif
