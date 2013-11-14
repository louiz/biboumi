/**
 * Just a very simple test suite, by hand, using assert()
 */

#include <assert.h>

#include <iostream>

#include <bridge/colors.hpp>
#include <utils/encoding.hpp>
#include <string.h>

#include <config/config.hpp>

#include <xmpp/jid.hpp>
#include <xmpp/xmpp_parser.hpp>

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

  std::string coucou("\u0002\u0002COUCOU\u0003");
  remove_irc_colors(coucou);
  assert(coucou == "COUCOU");

  /**
   * XML parsing
   */
  XmppParser xml;
  const std::string doc = "<stream xmlns='stream_ns'><stanza b='c'>inner<child1><grandchild/></child1><child2 xmlns='child2_ns'/>tail</stanza></stream>";
  xml.add_stanza_callback([](const Stanza& stanza)
      {
        assert(stanza.get_name() == "stream_ns:stanza");
        assert(stanza["b"] == "c");
        assert(stanza.get_inner() == "inner");
        assert(stanza.get_tail() == "");
        assert(stanza.get_child("stream_ns:child1") != nullptr);
        assert(stanza.get_child("stream_ns:child2") == nullptr);
        assert(stanza.get_child("child2_ns:child2") != nullptr);
        assert(stanza.get_child("child2_ns:child2")->get_tail() == "tail");
      });
  xml.feed(doc.data(), doc.size(), true);

  /**
   * JID parsing
   */
  // Full JID
  Jid jid1("♥@ツ.coucou/coucou@coucou/coucou");
  std::cerr << jid1.local << " @ " << jid1.domain << " / " << jid1.resource << std::endl;
  assert(jid1.local == "♥");
  assert(jid1.domain == "ツ.coucou");
  assert(jid1.resource == "coucou@coucou/coucou");

  // Domain and resource
  Jid jid2("ツ.coucou/coucou@coucou/coucou");
  std::cerr << jid2.local << " @ " << jid2.domain << " / " << jid2.resource << std::endl;
  assert(jid2.local == "");
  assert(jid2.domain == "ツ.coucou");
  assert(jid2.resource == "coucou@coucou/coucou");

  /**
   * Config
   */
  Config::filename = "test.cfg";
  Config::file_must_exist = false;
  Config::set("coucou", "bonjour");
  Config::close();

  bool error = false;
  try
    {
      Config::file_must_exist = true;
      assert(Config::get("coucou", "") == "bonjour");
      assert(Config::get("does not exist", "default") == "default");
      Config::close();
    }
  catch (const std::ios::failure& e)
    {
      error = true;
    }
  assert(error == false);
  return 0;
}
