#include "catch.hpp"

#include <logger/logger.hpp>
#include <config/config.hpp>

#include "io_tester.hpp"
#include <iostream>

using namespace std::string_literals;

TEST_CASE("Basic logging")
{
#ifdef SYSTEMD_FOUND
  const std::string debug_header = "<7>";
  const std::string error_header = "<3>";
#else
  const std::string debug_header = "[DEBUG]: ";
  const std::string error_header = "[ERROR]: ";
#endif
  Logger::instance().reset();
  GIVEN("A logger with log_level 0")
    {
      Config::set("log_level", "0");
      WHEN("we log some debug text")
        {
          IoTester<std::ostream> out(std::clog);
          log_debug("deb", "ug");
          THEN("debug logs are written")
            CHECK(out.str() == debug_header + "tests/logger.cpp:" + std::to_string(__LINE__ - 2) + ":\tdebug\n");
        }
      WHEN("we log some errors")
        {
          IoTester<std::ostream> out(std::clog);
          log_error("err", 12, "or");
          THEN("error logs are written")
            CHECK(out.str() == error_header + "tests/logger.cpp:" + std::to_string(__LINE__ - 2) + ":\terr12or\n");
        }
    }
  GIVEN("A logger with log_level 3")
    {
      Config::set("log_level", "3");
      WHEN("we log some debug text")
        {
          IoTester<std::ostream> out(std::clog);
          log_debug(123, "debug");
          THEN("nothing is written")
            CHECK(out.str().empty());
        }
      WHEN("we log some errors")
        {
          IoTester<std::ostream> out(std::clog);
          log_error(123, " errors");
          THEN("error logs are still written")
            CHECK(out.str() == error_header + "tests/logger.cpp:" + std::to_string(__LINE__ - 2) + ":\t123 errors\n");
        }
    }
}
