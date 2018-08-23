#include "catch.hpp"

#include <xmpp/jid.hpp>
#include <biboumi.h>

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
}

TEST_CASE("jidprep")
{
  // Jidprep
  const std::string badjid("~zigougou™@EpiK-7D9D1FDE.poez.io/Boujour/coucou/slt™");
#ifdef LIBIDN_FOUND
  const std::string correctjid = jidprep(badjid);
  CHECK(correctjid == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");
  // Check that the cache does not break things when we prep the same string
  // multiple times
  CHECK(jidprep(badjid) == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");
  CHECK(jidprep(badjid) == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");

  CHECK(jidprep("Zigougou@poez.io") == "zigougou@poez.io");

  CHECK(jidprep("~Bisous@88.123.43.45") == "~bisous@88.123.43.45");

  CHECK(jidprep("~Bisous@::ffff:42.156.139.46") == "~bisous@[::ffff:42.156.139.46]");

  CHECK(jidprep("louiz!6bf74289@2001:bc8:38e7::") == "louiz!6bf74289@[2001:bc8:38e7::]");

  CHECK(jidprep("louiz@+:::::----coucou.com78--.") == "louiz@coucou.com78");
  CHECK(jidprep("louiz@coucou.com78--.") == "louiz@coucou.com78");
  CHECK(jidprep("louiz@+:::::----coucou.com78") == "louiz@coucou.com78");
  CHECK(jidprep("louiz@:::::") == "louiz@empty");
#else // Without libidn, jidprep always returns an empty string
  CHECK(jidprep(badjid) == "");
#endif
}
