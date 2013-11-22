/**
 * Just a very simple test suite, by hand, using assert()
 */

#include <xmpp/xmpp_parser.hpp>
#include <utils/encoding.hpp>
#include <config/config.hpp>
#include <bridge/colors.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <string.h>

#include <iostream>
#include <vector>

#include <assert.h>

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

  /**
   * Utils
   */
  std::vector<std::string> splitted = utils::split("a::a", ':', false);
  assert(splitted.size() == 2);
  splitted = utils::split("a::a", ':', true);
  assert(splitted.size() == 3);
  assert(splitted[0] == "a");
  assert(splitted[1] == "");
  assert(splitted[2] == "a");
  splitted = utils::split("\na", '\n', true);
  assert(splitted.size() == 2);
  assert(splitted[0] == "");
  assert(splitted[1] == "a");

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
   * XML escape/escape
   */
  const std::string unescaped = "'coucou'<cc>/&\"gaga\"";
  assert(xml_escape(unescaped) == "&apos;coucou&apos;&lt;cc&gt;/&amp;&quot;gaga&quot;");
  assert(xml_unescape(xml_escape(unescaped)) == unescaped);

  /**
   * Colors conversion
   */
  std::unique_ptr<XmlNode> xhtml;
  std::string cleaned_up;

  std::tie(cleaned_up, xhtml) =
    irc_format_to_xhtmlim("normalboldunder-and-boldbold normal"
                          "5red,5default-on-red10,2cyan-on-blue");
  assert(xhtml);
  assert(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>normal<span style='font-weight:bold;'>bold</span><span style='font-weight:bold;text-decoration:underline;'>under-and-bold</span><span style='font-weight:bold;'>bold</span> normal<span style='color:red;'>red</span><span style='background-color:red;'>default-on-red</span><span style='color:cyan;background-color:blue;'>cyan-on-blue</span></body>");
  assert(cleaned_up == "normalboldunder-and-boldbold normalreddefault-on-redcyan-on-blue");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("normal");
  assert(!xhtml && cleaned_up == "normal");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("");
  assert(xhtml && !xhtml->has_children() && cleaned_up.empty());

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim(",a");
  assert(xhtml && !xhtml->has_children() && cleaned_up == "a");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim(",");
  assert(xhtml && !xhtml->has_children() && cleaned_up.empty());

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
