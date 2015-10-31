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

TEST_CASE("Config callbacks")
{
  bool switched = false;
  Config::connect([&switched]()
                  {
                    switched = !switched;
                  });
  CHECK_FALSE(switched);
  Config::set("un", "deux", true);
  CHECK(switched);
  Config::set("un", "trois", true);
  CHECK_FALSE(switched);

  Config::set("un", "trois", false);
  CHECK_FALSE(switched);
}

TEST_CASE("Config get_int")
{
  auto res = Config::get_int("number", 0);
  CHECK(res == 0);
  Config::set("number", "88");
  res = Config::get_int("number", 0);
  CHECK(res == 88);
  Config::set("number", "pouet");
  res = Config::get_int("number", -1);
  CHECK(res == 0);
}
