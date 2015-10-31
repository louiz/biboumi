#include "catch.hpp"

#include <database/database.hpp>

#include <unistd.h>
#include <config/config.hpp>

TEST_CASE("Database")
{
#ifdef USE_DATABASE
  // Remove any potential existing db
  ::unlink("./test.db");
  Config::set("db_name", "test.db");
  Database::set_verbose(false);
  auto o = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
  o.update();
  auto a = Database::get_irc_server_options("zouzou@example.com", "irc.example.com");
  auto b = Database::get_irc_server_options("moumou@example.com", "irc.example.com");

  // b does not yet exist in the db, the object is created but not yet
  // inserted
  CHECK(1 == Database::count<db::IrcServerOptions>());

  b.update();
  CHECK(2 == Database::count<db::IrcServerOptions>());

  CHECK(b.pass == "");
  CHECK(b.pass.value() == "");

  Database::close();
#endif
}
