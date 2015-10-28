#include "catch.hpp"

#include <irc/iid.hpp>
#include <irc/irc_user.hpp>

#include <config/config.hpp>

TEST_CASE("Irc user parsing")
{
  const std::map<char, char> prefixes{{'!', 'a'}, {'@', 'o'}};

  IrcUser user1("!nick!~some@host.bla", prefixes);
  CHECK(user1.nick == "nick");
  CHECK(user1.host == "~some@host.bla");
  CHECK(user1.modes.size() == 1);
  CHECK(user1.modes.find('a') != user1.modes.end());

  IrcUser user2("coucou!~other@host.bla", prefixes);
  CHECK(user2.nick == "coucou");
  CHECK(user2.host == "~other@host.bla");
  CHECK(user2.modes.empty());
  CHECK(user2.modes.find('a') == user2.modes.end());
}

/**
 * Let Catch know how to display Iid objects
 */
namespace Catch
{
  template<>
  struct StringMaker<Iid>
  {
    static std::string convert(const Iid& value)
    {
      return std::to_string(value);
    }
  };
}

TEST_CASE("Iid creation")
{
    Iid iid1("foo!irc.example.org");
    CHECK(std::to_string(iid1) == "foo!irc.example.org");
    CHECK(iid1.get_local() == "foo");
    CHECK(iid1.get_server() == "irc.example.org");
    CHECK(!iid1.is_channel);
    CHECK(iid1.is_user);

    Iid iid2("#test%irc.example.org");
    CHECK(std::to_string(iid2) == "#test%irc.example.org");
    CHECK(iid2.get_local() == "#test");
    CHECK(iid2.get_server() == "irc.example.org");
    CHECK(iid2.is_channel);
    CHECK(!iid2.is_user);

    Iid iid3("%irc.example.org");
    CHECK(std::to_string(iid3) == "%irc.example.org");
    CHECK(iid3.get_local() == "");
    CHECK(iid3.get_server() == "irc.example.org");
    CHECK(iid3.is_channel);
    CHECK(!iid3.is_user);

    Iid iid4("irc.example.org");
    CHECK(std::to_string(iid4) == "irc.example.org");
    CHECK(iid4.get_local() == "");
    CHECK(iid4.get_server() == "irc.example.org");
    CHECK(!iid4.is_channel);
    CHECK(!iid4.is_user);

    Iid iid5("nick!");
    CHECK(std::to_string(iid5) == "nick!");
    CHECK(iid5.get_local() == "nick");
    CHECK(iid5.get_server() == "");
    CHECK(!iid5.is_channel);
    CHECK(iid5.is_user);

    Iid iid6("##channel%");
    CHECK(std::to_string(iid6) == "##channel%");
    CHECK(iid6.get_local() == "##channel");
    CHECK(iid6.get_server() == "");
    CHECK(iid6.is_channel);
    CHECK(!iid6.is_user);
}

TEST_CASE("Iid creation in fixed_server mode")
{
    Config::set("fixed_irc_server", "fixed.example.com", false);

    Iid iid1("foo!irc.example.org");
    CHECK(std::to_string(iid1) == "foo!");
    CHECK(iid1.get_local() == "foo");
    CHECK(iid1.get_server() == "fixed.example.com");
    CHECK(!iid1.is_channel);
    CHECK(iid1.is_user);

    Iid iid2("#test%irc.example.org");
    CHECK(std::to_string(iid2) == "#test%irc.example.org");
    CHECK(iid2.get_local() == "#test%irc.example.org");
    CHECK(iid2.get_server() == "fixed.example.com");
    CHECK(iid2.is_channel);
    CHECK(!iid2.is_user);

    // Note that it is impossible to adress the IRC server directly, or to
    // use the virtual channel, in that mode

    // Iid iid3("%irc.example.org");
    // Iid iid4("irc.example.org");

    Iid iid5("nick!");
    CHECK(std::to_string(iid5) == "nick!");
    CHECK(iid5.get_local() == "nick");
    CHECK(iid5.get_server() == "fixed.example.com");
    CHECK(!iid5.is_channel);
    CHECK(iid5.is_user);

    Iid iid6("##channel%");
    CHECK(std::to_string(iid6) == "##channel%");
    CHECK(iid6.get_local() == "##channel%");
    CHECK(iid6.get_server() == "fixed.example.com");
    CHECK(iid6.is_channel);
    CHECK(!iid6.is_user);
}
