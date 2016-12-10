#include "catch.hpp"
#include "io_tester.hpp"

#include <iostream>

#include <config/config.hpp>

TEST_CASE("Config basic")
{
  // Disable all output for this test
  IoTester<std::ostream> out(std::cerr);
  // Write a value in the config file
  Config::read_conf("test.cfg");
  Config::set("coucou", "bonjour", true);
  Config::clear();

  bool error = false;
  try
    {
      CHECK(Config::read_conf());
      CHECK(Config::get("coucou", "") == "bonjour");
      CHECK(Config::get("does not exist", "default") == "default");
      Config::clear();
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
