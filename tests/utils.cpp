#include "catch.hpp"

#include <utils/tolower.hpp>
#include <utils/revstr.hpp>
#include <utils/string.hpp>
#include <utils/split.hpp>
#include <utils/xdg.hpp>
#include <utils/empty_if_fixed_server.hpp>
#include <utils/get_first_non_empty.hpp>
#include <utils/time.hpp>
#include <utils/system.hpp>
#include <utils/scopeguard.hpp>
#include <utils/dirname.hpp>
#include <utils/is_one_of.hpp>

using namespace std::string_literals;

TEST_CASE("String split")
{
  std::vector<std::string> splitted = utils::split("a::a", ':', false);
  CHECK(splitted.size() == 2);
  splitted = utils::split("a::a", ':', true);
  CHECK(splitted.size() == 3);
  CHECK(splitted[0] == "a");
  CHECK(splitted[1] == "");
  CHECK(splitted[2] == "a");
  splitted = utils::split("\na", '\n', true);
  CHECK(splitted.size() == 2);
  CHECK(splitted[0] == "");
  CHECK(splitted[1] == "a");
  splitted = utils::split("multi-prefix ", ' ');
  CHECK(splitted[0] == "multi-prefix");
  CHECK(splitted.size() == 1);
}

TEST_CASE("tolower")
{
  const std::string lowercase = utils::tolower("CoUcOu LeS CoPaiNs ♥");
  CHECK(lowercase == "coucou les copains ♥");

  const std::string ltr = "coucou";
  CHECK(utils::revstr(ltr) == "uocuoc");
}

TEST_CASE("to_bool")
{
  CHECK(to_bool("true"));
  CHECK(!to_bool("trou"));
  CHECK(to_bool("1"));
  CHECK(!to_bool("0"));
  CHECK(!to_bool("-1"));
  CHECK(!to_bool("false"));
}

TEST_CASE("xdg_*_path")
{
  ::unsetenv("XDG_CONFIG_HOME");
  ::unsetenv("HOME");
  std::string res;

  SECTION("Without XDG_CONFIG_HOME nor HOME")
    {
      res = xdg_config_path("coucou.txt");
      CHECK(res == "coucou.txt");
    }
  SECTION("With only HOME")
    {
      ::setenv("HOME", "/home/user", 1);
      res = xdg_config_path("coucou.txt");
      CHECK(res == "/home/user/.config/biboumi/coucou.txt");
    }
  SECTION("With only XDG_CONFIG_HOME")
    {
      ::setenv("XDG_CONFIG_HOME", "/some_weird_dir", 1);
      res = xdg_config_path("coucou.txt");
      CHECK(res == "/some_weird_dir/biboumi/coucou.txt");
    }
  SECTION("With XDG_DATA_HOME")
    {
      ::setenv("XDG_DATA_HOME", "/datadir", 1);
      res = xdg_data_path("bonjour.txt");
      CHECK(res == "/datadir/biboumi/bonjour.txt");
    }
}

TEST_CASE("empty if fixed irc server")
{
  GIVEN("A config with fixed_irc_server")
    {
      Config::set("fixed_irc_server", "irc.localhost");
      THEN("our string is made empty")
        CHECK(utils::empty_if_fixed_server("coucou coucou") == "");
    }
  GIVEN("A config with NO fixed_irc_server")
    {
      Config::set("fixed_irc_server", "");
      THEN("our string is returned untouched")
        CHECK(utils::empty_if_fixed_server("coucou coucou") == "coucou coucou");
    }

}

TEST_CASE("string cut")
{
  CHECK(cut("coucou", 2).size() == 3);
  CHECK(cut("bonjour les copains", 6).size() == 4);
  CHECK(cut("««««", 2).size() == 4);
  CHECK(cut("a««««", 2).size() == 5);
  const auto res = cut("rhello, ♥", 10);
  CHECK(res.size() == 2);
  CHECK(res[0] == "rhello, ");
  CHECK(res[1] == "♥");
}

TEST_CASE("first non-empty string")
{
  CHECK(get_first_non_empty(""s, ""s, "hello"s, "world"s) == "hello"s);
  CHECK(get_first_non_empty(""s, ""s, ""s, ""s) == ""s);
  CHECK(get_first_non_empty("first"s) == "first"s);
  CHECK(get_first_non_empty(0, 1, 2, 3) == 1);
}

TEST_CASE("time_to_string")
{
  const std::time_t stamp = 1472480968;
  const std::string result = "2016-08-29T14:29:28Z";
  CHECK(utils::to_string(stamp) == result);
  CHECK(utils::to_string(stamp).size() == result.size());
  CHECK(utils::to_string(0) == "1970-01-01T00:00:00Z");
}

TEST_CASE("parse_datetime")
{
  CHECK(utils::parse_datetime("1970-01-01T00:00:00Z") == 0);

  const int twenty_three_hours = 82800;
  CHECK(utils::parse_datetime("1970-01-01T23:00:12Z") == twenty_three_hours + 12);
  CHECK(utils::parse_datetime("1970-01-01T23:00:12Z") == utils::parse_datetime("1970-01-01T23:00:12+00:00"));
  CHECK(utils::parse_datetime("1970-01-01T23:00:12Z") == utils::parse_datetime("1970-01-01T23:00:12-00:00"));
  CHECK(utils::parse_datetime("1970-01-02T00:00:12Z") == utils::parse_datetime("1970-01-01T23:00:12-01:00"));
  CHECK(utils::parse_datetime("1970-01-02T00:00:12Z") == utils::parse_datetime("1970-01-02T01:00:12+01:00"));

  CHECK(utils::parse_datetime("2016-08-29T14:29:29Z") == 1472480969);

  CHECK(utils::parse_datetime("blah") == -1);
  CHECK(utils::parse_datetime("1970-01-02T00:00:12B") == -1);
  CHECK(utils::parse_datetime("1970-01-02T00:00:12*00:00") == -1);
  CHECK(utils::parse_datetime("1970-01-02T00:00:12+0000") == -1);
}

TEST_CASE("scope_guard")
{
  bool res = false;
  {
    auto guard = utils::make_scope_guard([&res](){ res = true; });
    CHECK(!res);
  }
  CHECK(res);
}

TEST_CASE("system_name")
{
  CHECK(utils::get_system_name() != "Unknown");
  CHECK(!utils::get_system_name().empty());
}

TEST_CASE("dirname")
{
  CHECK(utils::dirname("/") == "/");
  CHECK(utils::dirname("coucou.txt") == "./");
  CHECK(utils::dirname("../coucou.txt") == "../");
  CHECK(utils::dirname("/etc/biboumi/coucou.txt") == "/etc/biboumi/");
  CHECK(utils::dirname("..") == "..");
  CHECK(utils::dirname("../") == "../");
  CHECK(utils::dirname(".") == ".");
  CHECK(utils::dirname("./") == "./");
}

TEST_CASE("is_in")
{
  CHECK((is_one_of<int, float, std::string, int>) == true);
  CHECK((is_one_of<int, float, std::string>) == false);
  CHECK((is_one_of<int>) == false);
  CHECK((is_one_of<int, int>) == true);
  CHECK((is_one_of<bool, int>) == false);
  CHECK((is_one_of<bool, bool>) == true);
  CHECK((is_one_of<bool, bool, bool, bool, bool, int>) == true);
}
