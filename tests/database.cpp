#include "catch.hpp"

#include "io_tester.hpp"
#include <database/database.hpp>

#include <config/config.hpp>

TEST_CASE("Database")
{
  IoTester<std::ostream> out(std::cout);
#ifdef USE_DATABASE
  Database::open(":memory:");
  Database::set_verbose(false);

  SECTION("Basic retrieve and update")
    {
      auto o = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
      Database::persist(o);
      auto a = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
      auto b = Database::get_irc_server_options("moumou@example.com", "irc.example.com");

      // b does not yet exist in the db, the object is created but not yet
      // inserted
      CHECK(1 == Database::count<db::IrcServerOptions>());

      Database::persist(b);
      CHECK(2 == Database::count<db::IrcServerOptions>());

      CHECK(b.pass == "");
    }

  SECTION("channel options")
    {
      Config::set("db_name", ":memory:");
      auto o = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");

      CHECK(o.encoding_in == "");
      o.encoding_in = "latin-1";
      Database::persist(o);
      auto b = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");
      CHECK(o.encoding_in == "latin-1");
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
        c.encoding_in = "channelEncoding";
        Database::persist(c);
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the channel option")
              CHECK(r.encoding_in == "channelEncoding");
          }
      }
      GIVEN("An option defined for the server but not the channel")
        {
          s.encoding_in = "serverEncoding";
          Database::persist(s);
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the server option")
              CHECK(r.encoding_in == "serverEncoding");
          }
        }
      GIVEN("An option defined for both the server and the channel")
        {
          s.encoding_in = "serverEncoding";
          Database::persist(s);
          c.encoding_in = "channelEncoding";
          Database::persist(c);
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the channel option")
              CHECK(r.encoding_in == "channelEncoding");
          }
        WHEN("we fetch that option, with no channel specified")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, "");
            THEN("we get the server option")
              CHECK(r.encoding_in == "serverEncoding");
          }
        }
    }

  Database::close();
#endif
}
