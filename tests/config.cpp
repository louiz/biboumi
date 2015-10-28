#include "catch.hpp"

#include <config/config.hpp>

TEST_CASE("Config basic")
{
  Config::filename = "test.cfg";
  Config::file_must_exist = false;
  Config::set("coucou", "bonjour", true);
  Config::close();

  bool error = false;
  try
    {
      Config::file_must_exist = true;
      CHECK(Config::get("coucou", "") == "bonjour");
      CHECK(Config::get("does not exist", "default") == "default");
      Config::close();
    }
  catch (const std::ios::failure& e)
    {
      error = true;
    }
  CHECK_FALSE(error);
}
