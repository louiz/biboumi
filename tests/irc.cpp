#include "catch2/catch.hpp"

#include <irc/irc_message.hpp>

TEST_CASE("Basic IRC message parsing")
{
  IrcMessage m(":prefix COMMAND un deux trois");
  CHECK(m.prefix == "prefix");
  CHECK(m.command == "COMMAND");
  CHECK(m.arguments.size() == 3);
  CHECK(m.arguments[0] == "un");
  CHECK(m.arguments[1] == "deux");
  CHECK(m.arguments[2] == "trois");
}

TEST_CASE("Trailing space")
{
  IrcMessage m(":prefix COMMAND un deux trois ");
  CHECK(m.prefix == "prefix");
  CHECK(m.arguments.size() == 3);
  CHECK(m.arguments[0] == "un");
  CHECK(m.arguments[1] == "deux");
  CHECK(m.arguments[2] == "trois");
}

TEST_CASE("Message with :")
{
  IrcMessage m(":prefix COMMAND un :coucou les amis ");
  CHECK(m.prefix == "prefix");
  CHECK(m.arguments.size() == 2);
  CHECK(m.arguments[0] == "un");
  CHECK(m.arguments[1] == "coucou les amis ");
}

TEST_CASE("Message with empty :")
{
  IrcMessage m("COMMAND un deux :");
  CHECK(m.prefix == "");
  CHECK(m.arguments.size() == 3);
  CHECK(m.arguments[0] == "un");
  CHECK(m.arguments[1] == "deux");
  CHECK(m.arguments[2] == "");
}
