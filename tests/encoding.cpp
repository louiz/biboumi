#include "catch2/catch.hpp"

#include <utils/encoding.hpp>


TEST_CASE("UTF-8 validation")
{
  const char* valid = "C̡͔͕̩͙̽ͫ̈́ͥ̿̆ͧ̚r̸̩̘͍̻͖̆͆͛͊̉̕͡o͇͈̳̤̱̊̈͢q̻͍̦̮͕ͥͬͬ̽ͭ͌̾ͅǔ͉͕͇͚̙͉̭͉̇̽ȇ͈̮̼͍͔ͣ͊͞͝ͅ ͫ̾ͪ̓ͥ̆̋̔҉̢̦̠͈͔̖̲̯̦ụ̶̯͐̃̋ͮ͆͝n̬̱̭͇̻̱̰̖̤̏͛̏̿̑͟ë́͐҉̸̥̪͕̹̻̙͉̰ ̹̼̱̦̥ͩ͑̈́͑͝ͅt͍̥͈̹̝ͣ̃̔̈̔ͧ̕͝ḙ̸̖̟̙͙ͪ͢ų̯̞̼̲͓̻̞͛̃̀́b̮̰̗̩̰̊̆͗̾̎̆ͯ͌͝.̗̙͎̦ͫ̈́ͥ͌̈̓ͬ";
  CHECK(utils::is_valid_utf8(valid));
  CHECK_FALSE(utils::is_valid_utf8("\xF0\x0F"));
  CHECK_FALSE(utils::is_valid_utf8("\xFE\xFE\xFF\xFF"));

  std::string in = "Biboumi ╯°□°）╯︵ ┻━┻";
  CHECK(utils::is_valid_utf8(in.data()));
}

TEST_CASE("UTF-8 conversion")
{
  std::string in = "Biboumi ╯°□°）╯︵ ┻━┻";
  REQUIRE(utils::is_valid_utf8(in.data()));

  SECTION("Converting UTF-8 to UTF-8 should return the same string")
    {
      std::string res = utils::convert_to_utf8(in, "UTF-8");
      CHECK(utils::is_valid_utf8(res.c_str()) == true);
      CHECK(res == in);
    }

  SECTION("Testing latin-1 conversion")
    {
      std::string original_utf8("couc¥ou");
      std::string original_latin1("couc\xa5ou");

      SECTION("Convert proper latin-1 to UTF-8")
        {
          std::string from_latin1 = utils::convert_to_utf8(original_latin1.c_str(), "ISO-8859-1");
          CHECK(from_latin1 == original_utf8);
        }
      SECTION("Check the behaviour when the decoding fails (here because we provide a wrong charset)")
        {
          std::string from_ascii = utils::convert_to_utf8(original_latin1, "US-ASCII");
          CHECK(from_ascii == "couc�ou");
        }
    }
}

TEST_CASE("Remove invalid XML chars")
{
  std::string without_ctrl_char("𤭢€¢$");
  std::string in = "Biboumi ╯°□°）╯︵ ┻━┻";
  CHECK(utils::remove_invalid_xml_chars(without_ctrl_char) == without_ctrl_char);
  CHECK(utils::remove_invalid_xml_chars(in) == in);
  CHECK(utils::remove_invalid_xml_chars("\acouco\u0008u\uFFFEt\uFFFFe\r\n♥") == "coucoute\r\n♥");
}
