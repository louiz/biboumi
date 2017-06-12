#include "catch.hpp"

#include <database/database.hpp>

#include <config/config.hpp>

TEST_CASE("Database")
{
#ifdef USE_DATABASE
  Database::open(":memory:");

  SECTION("Basic retrieve and update")
    {
      auto o = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
      o.save(Database::db);
      auto a = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
      auto b = Database::get_irc_server_options("moumou@example.com", "irc.example.com");

      // b does not yet exist in the db, the object is created but not yet
      // inserted
      CHECK(1 == Database::count<db::IrcServerOptions>());

      b.update();
      CHECK(2 == Database::count<db::IrcServerOptions>());

      CHECK(b.pass == "");
      CHECK(b.pass.value() == "");
    }

  SECTION("channel options")
    {
      Config::set("db_name", ":memory:");
      auto o = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");

      CHECK(o.encodingIn == "");
      o.encodingIn = "ISO-8859-1";
      o.update();
      auto b = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");
      CHECK(o.encodingIn == "ISO-8859-1");
    }

  SECTION("Channel options with server default")
    {
      const std::string owner{"zouzou@example.com"};
      const std::string server{"irc.example.com"};
      const std::string chan1{"#foo"};

      auto c = Database::get_irc_channel_options(owner, server, chan1);
      auto s = Database::get_irc_server_options(owner, server);

      GIVEN("An option defined for the channel but not the server")
      {
        c.encodingIn = "channelEncoding";
        c.update();
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the channel option")
              CHECK(r.encodingIn == "channelEncoding");
          }
      }
      GIVEN("An option defined for the server but not the channel")
        {
          s.encodingIn = "serverEncoding";
          s.update();
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the server option")
              CHECK(r.encodingIn == "serverEncoding");
          }
        }
      GIVEN("An option defined for both the server and the channel")
        {
          s.encodingIn = "serverEncoding";
          s.update();
          c.encodingIn = "channelEncoding";
          c.update();
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the channel option")
              CHECK(r.encodingIn == "channelEncoding");
          }
        WHEN("we fetch that option, with no channel specified")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, "");
            THEN("we get the server option")
              CHECK(r.encodingIn == "serverEncoding");
          }
        }
    }

  Database::close();
#endif
}
