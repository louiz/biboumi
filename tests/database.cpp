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
      CHECK(1 == Database::count(Database::irc_server_options));

      b.save(Database::db);
      CHECK(2 == Database::count(Database::irc_server_options));

      CHECK(b.col<Database::Pass>() == "");
    }

  SECTION("channel options")
    {
      Config::set("db_name", ":memory:");
      auto o = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");

      CHECK(o.col<Database::EncodingIn>() == "");
      o.col<Database::EncodingIn>() = "ISO-8859-1";
      CHECK(o.col<Database::RecordHistoryOptional>().is_set == false);
      o.col<Database::RecordHistoryOptional>().set_value(false);
      o.save(Database::db);
      auto b = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");
      CHECK(o.col<Database::EncodingIn>() == "ISO-8859-1");
      CHECK(o.col<Database::RecordHistoryOptional>().is_set == true);
      CHECK(o.col<Database::RecordHistoryOptional>().value == false);
    }

  SECTION("Channel options with server default")
    {
      const std::string owner{"CACA@example.com"};
      const std::string server{"irc.example.com"};
      const std::string chan1{"#foo"};

      auto c = Database::get_irc_channel_options(owner, server, chan1);
      auto s = Database::get_irc_server_options(owner, server);

      GIVEN("An option defined for the channel but not the server")
      {
        c.col<Database::EncodingIn>() = "channelEncoding";
        c.save(Database::db);
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the channel option")
              CHECK(r.col<Database::EncodingIn>() == "channelEncoding");
          }
      }
      GIVEN("An option defined for the server but not the channel")
        {
          s.col<Database::EncodingIn>() = "serverEncoding";
          s.save(Database::db);
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the server option")
              CHECK(r.col<Database::EncodingIn>() == "serverEncoding");
          }
        }
      GIVEN("An option defined for both the server and the channel")
        {
          s.col<Database::EncodingIn>() = "serverEncoding";
          s.save(Database::db);
          c.col<Database::EncodingIn>() = "channelEncoding";
          c.save(Database::db);
        WHEN("we fetch that option")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, chan1);
            THEN("we get the channel option")
              CHECK(r.col<Database::EncodingIn>() == "channelEncoding");
          }
        WHEN("we fetch that option, with no channel specified")
          {
            auto r = Database::get_irc_channel_options_with_server_default(owner, server, "");
            THEN("we get the server option")
              CHECK(r.col<Database::EncodingIn>() == "serverEncoding");
          }
        }
    }

  Database::close();
#endif
}
