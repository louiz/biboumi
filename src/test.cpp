/**
 * Just a very simple test suite, by hand, using assert()
 */

#include <assert.h>

#include <iostream>

#include <utils/encoding.hpp>
#include <string.h>

#include <fstream>

int main()
{
  /**
   * Encoding
   */
  const char* valid = "C̡͔͕̩͙̽ͫ̈́ͥ̿̆ͧ̚r̸̩̘͍̻͖̆͆͛͊̉̕͡o͇͈̳̤̱̊̈͢q̻͍̦̮͕ͥͬͬ̽ͭ͌̾ͅǔ͉͕͇͚̙͉̭͉̇̽ȇ͈̮̼͍͔ͣ͊͞͝ͅ ͫ̾ͪ̓ͥ̆̋̔҉̢̦̠͈͔̖̲̯̦ụ̶̯͐̃̋ͮ͆͝n̬̱̭͇̻̱̰̖̤̏͛̏̿̑͟ë́͐҉̸̥̪͕̹̻̙͉̰ ̹̼̱̦̥ͩ͑̈́͑͝ͅt͍̥͈̹̝ͣ̃̔̈̔ͧ̕͝ḙ̸̖̟̙͙ͪ͢ų̯̞̼̲͓̻̞͛̃̀́b̮̰̗̩̰̊̆͗̾̎̆ͯ͌͝.̗̙͎̦ͫ̈́ͥ͌̈̓ͬ";
  assert(utils::is_valid_utf8(valid) == true);
  const char* invalid = "\xF0\x0F";
  assert(utils::is_valid_utf8(invalid) == false);
  const char* invalid2 = "\xFE\xFE\xFF\xFF";
  assert(utils::is_valid_utf8(invalid2) == false);

  std::string in = "coucou les copains  ♥ ";
  assert(utils::is_valid_utf8(in.c_str()) == true);
  std::string res = utils::convert_to_utf8(in, "UTF-8");
  assert(utils::is_valid_utf8(res.c_str()) == true && res == in);

  std::string original_utf8("couc¥ou");
  std::string original_latin1("couc\xa5ou");

  // When converting back to utf-8
  std::string from_latin1 = utils::convert_to_utf8(original_latin1.c_str(), "ISO-8859-1");
  assert(from_latin1 == original_utf8);

  // Check the behaviour when the decoding fails (here because we provide a
  // wrong charset)
  std::string from_ascii = utils::convert_to_utf8(original_latin1, "US-ASCII");
  assert(from_ascii == "couc�ou");
  return 0;
}
