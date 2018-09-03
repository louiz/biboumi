#include "catch.hpp"

#include <biboumi.h>

#ifdef USE_DATABASE

#include <cstdlib>

#include <database/database.hpp>
#include <database/save.hpp>

#include <config/config.hpp>

TEST_CASE("Database")
{
#ifdef PQ_FOUND
  std::string postgresql_uri{"postgresql://"};
  const char* env_value = ::getenv("TEST_POSTGRES_URI");
  if (env_value != nullptr)
    Database::open("postgresql://"s + env_value);
  else
#endif
    Database::open(":memory:");

  Database::raw_exec("DELETE FROM " + Database::irc_server_options.get_name());
  Database::raw_exec("DELETE FROM " + Database::irc_channel_options.get_name());

  SECTION("Basic retrieve and update")
    {
      auto o = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
      CHECK(Database::count(Database::irc_server_options) == 0);
      save(o, *Database::db);
      CHECK(Database::count(Database::irc_server_options) == 1);
      o.col<Database::Realname>() = "Different realname";
      CHECK(o.col<Database::Realname>() == "Different realname");
      save(o, *Database::db);
      CHECK(o.col<Database::Realname>() == "Different realname");
      CHECK(Database::count(Database::irc_server_options) == 1);

      auto a = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
      CHECK(a.col<Database::Realname>() == "Different realname");
      auto b = Database::get_irc_server_options("moumou@example.com", "irc.example.com");

      // b does not yet exist in the db, the object is created but not yet
      // inserted
      CHECK(1 == Database::count(Database::irc_server_options));

      save(b, *Database::db);
      CHECK(2 == Database::count(Database::irc_server_options));

      CHECK(b.col<Database::Pass>() == "");
    }

  SECTION("channel options")
    {
      auto o = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");

      CHECK(o.col<Database::EncodingIn>() == "");
      o.col<Database::EncodingIn>() = "ISO-8859-1";
      CHECK(o.col<Database::RecordHistoryOptional>().is_set == false);
      o.col<Database::RecordHistoryOptional>().set_value(false);
      save(o, *Database::db);
      auto b = Database::get_irc_channel_options("zouzou@example.com", "irc.example.com", "#foo");
      CHECK(o.col<Database::EncodingIn>() == "ISO-8859-1");
      CHECK(o.col<Database::RecordHistoryOptional>().is_set == true);
      CHECK(o.col<Database::RecordHistoryOptional>().value == false);

      o.clear();
      CHECK(o.col<Database::EncodingIn>() == "");
      CHECK(o.col<Database::Owner>() == "zouzou@example.com");
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
        save(c, *Database::db);
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
          save(s, *Database::db);
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
          save(s, *Database::db);
          c.col<Database::EncodingIn>() = "channelEncoding";
          save(c, *Database::db);
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

  SECTION("Server options")
    {
      const std::string owner{"toto@example.com"};
      const std::string owner2{"toto2@example.com"};
      const std::string server{"irc.example.com"};

      auto soptions = Database::get_irc_server_options(owner, server);
      auto soptions2 = Database::get_irc_server_options(owner2, server);

      auto after_connection_commands =  Database::get_after_connection_commands(soptions);
      CHECK(after_connection_commands.empty());

      save(soptions, *Database::db);
      save(soptions2, *Database::db);
      auto com = Database::after_connection_commands.row();
      com.col<Database::AfterConnectionCommand>() = "first";
      after_connection_commands.push_back(com);
      com.col<Database::AfterConnectionCommand>() = "second";
      after_connection_commands.push_back(com);
      Database::set_after_connection_commands(soptions, after_connection_commands);

      after_connection_commands.clear();
      com.col<Database::AfterConnectionCommand>() = "first";
      after_connection_commands.push_back(com);
      com.col<Database::AfterConnectionCommand>() = "second";
      after_connection_commands.push_back(com);
      Database::set_after_connection_commands(soptions2, after_connection_commands);

      after_connection_commands =  Database::get_after_connection_commands(soptions);
      CHECK(after_connection_commands.size() == 2);
      after_connection_commands =  Database::get_after_connection_commands(soptions2);
      CHECK(after_connection_commands.size() == 2);

      after_connection_commands.clear();
      after_connection_commands.push_back(com);
      Database::set_after_connection_commands(soptions, after_connection_commands);

      after_connection_commands =  Database::get_after_connection_commands(soptions);
      CHECK(after_connection_commands.size() == 1);
      after_connection_commands =  Database::get_after_connection_commands(soptions2);
      CHECK(after_connection_commands.size() == 2);
    }

  Database::close();
}
#endif
