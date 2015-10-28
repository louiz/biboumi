#include "catch.hpp"

#include <xmpp/jid.hpp>
#include <louloulibs.h>

TEST_CASE("Jid")
{
  Jid jid1("♥@ツ.coucou/coucou@coucou/coucou");
  CHECK(jid1.local == "♥");
  CHECK(jid1.domain == "ツ.coucou");
  CHECK(jid1.resource == "coucou@coucou/coucou");

  // Domain and resource
  Jid jid2("ツ.coucou/coucou@coucou/coucou");
  CHECK(jid2.local == "");
  CHECK(jid2.domain == "ツ.coucou");
  CHECK(jid2.resource == "coucou@coucou/coucou");

  // Jidprep
  const std::string badjid("~zigougou™@EpiK-7D9D1FDE.poez.io/Boujour/coucou/slt™");
  const std::string correctjid = jidprep(badjid);
#ifdef LIBIDN_FOUND
  CHECK(correctjid == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");
  // Check that the cache does not break things when we prep the same string
  // multiple times
  CHECK(jidprep(badjid) == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");
  CHECK(jidprep(badjid) == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");

  const std::string badjid2("Zigougou@poez.io");
  const std::string correctjid2 = jidprep(badjid2);
  CHECK(correctjid2 == "zigougou@poez.io");

  const std::string crappy("~Bisous@7ea8beb1:c5fd2849:da9a048e:ip");
  const std::string fixed_crappy = jidprep(crappy);
  CHECK(fixed_crappy == "~bisous@7ea8beb1-c5fd2849-da9a048e-ip");
#else // Without libidn, jidprep always returns an empty string
  CHECK(jidprep(badjid) == "");
#endif
}
