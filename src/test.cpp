/**
 * Just a very simple test suite, by hand, using assert()
 */

#include <xmpp/xmpp_component.hpp>
#include <utils/timed_events.hpp>
#include <xmpp/xmpp_parser.hpp>
#include <utils/encoding.hpp>
#include <logger/logger.hpp>
#include <config/config.hpp>
#include <bridge/colors.hpp>
#include <utils/tolower.hpp>
#include <utils/revstr.hpp>
#include <irc/irc_user.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <irc/iid.hpp>
#include <string.h>

#include <iostream>
#include <thread>
#include <vector>

#undef NDEBUG
#include <assert.h>

static const std::string color("[35m");
static const std::string reset("[m");

int main()
{

  /**
   * Config
   */
  std::cout << color << "Testing config…" << reset << std::endl;
  Config::filename = "test.cfg";
  Config::file_must_exist = false;
  Config::set("coucou", "bonjour", true);
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

  Config::set("log_level", "2");
  Config::set("log_file", "");

  std::cout << color << "Testing logging…" << reset << std::endl;
  log_debug("If you see this, the test FAILED.");
  log_info("If you see this, the test FAILED.");
  log_warning("You must see this message. And the next one too.");
  log_error("It’s not an error, don’t worry, the test passed.");


  /**
   * Timed events
   */
  std::cout << color << "Testing timed events…" << reset << std::endl;
  // No event.
  assert(TimedEventsManager::instance().get_timeout() == utils::no_timeout);
  assert(TimedEventsManager::instance().execute_expired_events() == 0);

  // Add a single event
  TimedEventsManager::instance().add_event(TimedEvent(std::chrono::steady_clock::now() + 50ms, [](){ std::cout << "Timeout expired" << std::endl; }));
  // The event should not yet be expired
  assert(TimedEventsManager::instance().get_timeout() > 0ms);
  assert(TimedEventsManager::instance().execute_expired_events() == 0);
  std::chrono::milliseconds timoute = TimedEventsManager::instance().get_timeout();
  std::cout << "Sleeping for " << timoute.count() << "ms" << std::endl;
  std::this_thread::sleep_for(timoute);

  // Event is now expired
  assert(TimedEventsManager::instance().execute_expired_events() == 1);
  assert(TimedEventsManager::instance().get_timeout() == utils::no_timeout);

  // Test canceling events
  TimedEventsManager::instance().add_event(TimedEvent(std::chrono::steady_clock::now() + 100ms, [](){ }, "un"));
  TimedEventsManager::instance().add_event(TimedEvent(std::chrono::steady_clock::now() + 200ms, [](){ }, "deux"));
  TimedEventsManager::instance().add_event(TimedEvent(std::chrono::steady_clock::now() + 300ms, [](){ }, "deux"));
  assert(TimedEventsManager::instance().get_timeout() > 0ms);
  assert(TimedEventsManager::instance().size() == 3);
  assert(TimedEventsManager::instance().cancel("un") == 1);
  assert(TimedEventsManager::instance().size() == 2);
  assert(TimedEventsManager::instance().cancel("deux") == 2);
  assert(TimedEventsManager::instance().get_timeout() == utils::no_timeout);

  /**
   * Encoding
   */
  std::cout << color << "Testing encoding…" << reset << std::endl;
  const char* valid = "C̡͔͕̩͙̽ͫ̈́ͥ̿̆ͧ̚r̸̩̘͍̻͖̆͆͛͊̉̕͡o͇͈̳̤̱̊̈͢q̻͍̦̮͕ͥͬͬ̽ͭ͌̾ͅǔ͉͕͇͚̙͉̭͉̇̽ȇ͈̮̼͍͔ͣ͊͞͝ͅ ͫ̾ͪ̓ͥ̆̋̔҉̢̦̠͈͔̖̲̯̦ụ̶̯͐̃̋ͮ͆͝n̬̱̭͇̻̱̰̖̤̏͛̏̿̑͟ë́͐҉̸̥̪͕̹̻̙͉̰ ̹̼̱̦̥ͩ͑̈́͑͝ͅt͍̥͈̹̝ͣ̃̔̈̔ͧ̕͝ḙ̸̖̟̙͙ͪ͢ų̯̞̼̲͓̻̞͛̃̀́b̮̰̗̩̰̊̆͗̾̎̆ͯ͌͝.̗̙͎̦ͫ̈́ͥ͌̈̓ͬ";
  assert(utils::is_valid_utf8(valid) == true);
  const char* invalid = "\xF0\x0F";
  assert(utils::is_valid_utf8(invalid) == false);
  const char* invalid2 = "\xFE\xFE\xFF\xFF";
  assert(utils::is_valid_utf8(invalid2) == false);

  std::string in = "Biboumi ╯°□°）╯︵ ┻━┻";
  std::cout << in << std::endl;
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
  std::cout << from_ascii << std::endl;

  std::string without_ctrl_char("𤭢€¢$");
  assert(utils::remove_invalid_xml_chars(without_ctrl_char) == without_ctrl_char);
  assert(utils::remove_invalid_xml_chars(in) == in);
  assert(utils::remove_invalid_xml_chars("\acouco\u0008u\uFFFEt\uFFFFe\r\n♥") == "coucoute\r\n♥");

  /**
   * Id generation
   */
  std::cout << color << "Testing id generation…" << reset << std::endl;
  const std::string first_uuid = XmppComponent::next_id();
  const std::string second_uuid = XmppComponent::next_id();
  std::cout << first_uuid << std::endl;
  std::cout << second_uuid << std::endl;
  assert(first_uuid.size() == 36);
  assert(second_uuid.size() == 36);
  assert(first_uuid != second_uuid);

  /**
   * Utils
   */
  std::cout << color << "Testing utils…" << reset << std::endl;
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

  const std::string lowercase = utils::tolower("CoUcOu LeS CoPaiNs ♥");
  std::cout << lowercase << std::endl;
  assert(lowercase == "coucou les copains ♥");

  const std::string ltr = "coucou";
  assert(utils::revstr(ltr) == "uocuoc");

  /**
   * XML parsing
   */
  std::cout << color << "Testing XML parsing…" << reset << std::endl;
  XmppParser xml;
  const std::string doc = "<stream xmlns='stream_ns'><stanza b='c'>inner<child1><grandchild/></child1><child2 xmlns='child2_ns'/>tail</stanza></stream>";
  auto check_stanza = [](const Stanza& stanza)
    {
      assert(stanza.get_name() == "stanza");
      assert(stanza.get_tag("xmlns") == "stream_ns");
      assert(stanza.get_tag("b") == "c");
      assert(stanza.get_inner() == "inner");
      assert(stanza.get_tail() == "");
      assert(stanza.get_child("child1", "stream_ns") != nullptr);
      assert(stanza.get_child("child2", "stream_ns") == nullptr);
      assert(stanza.get_child("child2", "child2_ns") != nullptr);
      assert(stanza.get_child("child2", "child2_ns")->get_tail() == "tail");
    };
  xml.add_stanza_callback([check_stanza](const Stanza& stanza)
      {
        std::cout << stanza.to_string() << std::endl;
        check_stanza(stanza);
        // Do the same checks on a copy of that stanza.
        Stanza copy(stanza);
        check_stanza(copy);
      });
  xml.feed(doc.data(), doc.size(), true);

  const std::string doc2 = "<stream xmlns='s'><stanza>coucou\r\n\a</stanza></stream>";
  xml.add_stanza_callback([](const Stanza& stanza)
      {
        std::cout << stanza.to_string() << std::endl;
        assert(stanza.get_inner() == "coucou\r\n");
      });
  xml.feed(doc2.data(), doc.size(), true);

  /**
   * XML escape/escape
   */
  std::cout << color << "Testing XML escaping…" << reset << std::endl;
  const std::string unescaped = "'coucou'<cc>/&\"gaga\"";
  assert(xml_escape(unescaped) == "&apos;coucou&apos;&lt;cc&gt;/&amp;&quot;gaga&quot;");
  assert(xml_unescape(xml_escape(unescaped)) == unescaped);

  /**
   * Irc user parsing
   */
  const std::map<char, char> prefixes{{'!', 'a'}, {'@', 'o'}};

  IrcUser user1("!nick!~some@host.bla", prefixes);
  assert(user1.nick == "nick");
  assert(user1.host == "~some@host.bla");
  assert(user1.modes.size() == 1);
  assert(user1.modes.find('a') != user1.modes.end());
  IrcUser user2("coucou!~other@host.bla", prefixes);
  assert(user2.nick == "coucou");
  assert(user2.host == "~other@host.bla");
  assert(user2.modes.empty());
  assert(user2.modes.find('a') == user2.modes.end());

  /**
   * Colors conversion
   */
  std::cout << color << "Testing IRC colors conversion…" << reset << std::endl;
  std::unique_ptr<XmlNode> xhtml;
  std::string cleaned_up;

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("bold");
  std::cout << xhtml->to_string() << std::endl;
  assert(xhtml && xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'><span style='font-weight:bold;'>bold</span></body>");

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

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("[\x1D13dolphin-emu/dolphin\x1D] 03foo commented on #283 (Add support for the guide button to XInput): 02http://example.com");
  assert(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>[<span style='font-style:italic;'/><span style='font-style:italic;color:lightmagenta;'>dolphin-emu/dolphin</span><span style='color:lightmagenta;'>] </span><span style='color:green;'>foo</span> commented on #283 (Add support for the guide button to XInput): <span style='text-decoration:underline;'/><span style='text-decoration:underline;color:blue;'>http://example.com</span><span style='text-decoration:underline;'/></body>");
  assert(cleaned_up == "[dolphin-emu/dolphin] foo commented on #283 (Add support for the guide button to XInput): http://example.com");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("0e46ab by 03Pierre Dindon [090|091|040] 02http://example.net/Ojrh4P media: avoid pop-in effect when loading thumbnails by specifying an explicit size");
  assert(cleaned_up == "0e46ab by Pierre Dindon [0|1|0] http://example.net/Ojrh4P media: avoid pop-in effect when loading thumbnails by specifying an explicit size");
  assert(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>0e46ab by <span style='color:green;'>Pierre Dindon</span> [<span style='color:lightgreen;'>0</span>|<span style='color:lightgreen;'>1</span>|<span style='color:indianred;'>0</span>] <span style='text-decoration:underline;'/><span style='text-decoration:underline;color:blue;'>http://example.net/Ojrh4P</span><span style='text-decoration:underline;'/> media: avoid pop-in effect when loading thumbnails by specifying an explicit size</body>");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("test\ncoucou");
  assert(cleaned_up == "test\ncoucou");
  assert(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>test<br/>coucou</body>");

  /**
   * JID parsing
   */
  std::cout << color << "Testing JID parsing…" << reset << std::endl;
  // Full JID
  Jid jid1("♥@ツ.coucou/coucou@coucou/coucou");
  std::cout << jid1.local << "@" << jid1.domain << "/" << jid1.resource << std::endl;
  assert(jid1.local == "♥");
  assert(jid1.domain == "ツ.coucou");
  assert(jid1.resource == "coucou@coucou/coucou");

  // Domain and resource
  Jid jid2("ツ.coucou/coucou@coucou/coucou");
  std::cout << jid2.local << "@" << jid2.domain << "/" << jid2.resource << std::endl;
  assert(jid2.local == "");
  assert(jid2.domain == "ツ.coucou");
  assert(jid2.resource == "coucou@coucou/coucou");

  // Jidprep
  const std::string& badjid("~zigougou™@EpiK-7D9D1FDE.poez.io/Boujour/coucou/slt™");
  const std::string correctjid = jidprep(badjid);
  std::cout << correctjid << std::endl;
  assert(correctjid == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");
  // Check that the cache do not break things when we prep the same string
  // multiple times
  assert(jidprep(badjid) == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");
  assert(jidprep(badjid) == "~zigougoutm@epik-7d9d1fde.poez.io/Boujour/coucou/sltTM");

  const std::string& badjid2("Zigougou@poez.io");
  const std::string correctjid2 = jidprep(badjid2);
  std::cout << correctjid2 << std::endl;
  assert(correctjid2 == "zigougou@poez.io");

  /**
   * IID parsing
   */
  std::cout << color << "Testing IID parsing…" << reset << std::endl;
  Iid iid1("foo!irc.example.org");
  std::cout << std::to_string(iid1) << std::endl;
  assert(std::to_string(iid1) == "foo!irc.example.org");
  assert(iid1.get_local() == "foo");
  assert(iid1.get_server() == "irc.example.org");
  assert(!iid1.is_channel);
  assert(iid1.is_user);

  Iid iid2("#test%irc.example.org");
  std::cout << std::to_string(iid2) << std::endl;
  assert(std::to_string(iid2) == "#test%irc.example.org");
  assert(iid2.get_local() == "#test");
  assert(iid2.get_server() == "irc.example.org");
  assert(iid2.is_channel);
  assert(!iid2.is_user);

  Iid iid3("%irc.example.org");
  std::cout << std::to_string(iid3) << std::endl;
  assert(std::to_string(iid3) == "%irc.example.org");
  assert(iid3.get_local() == "");
  assert(iid3.get_server() == "irc.example.org");
  assert(iid3.is_channel);
  assert(!iid3.is_user);

  Iid iid4("irc.example.org");
  std::cout << std::to_string(iid4) << std::endl;
  assert(std::to_string(iid4) == "irc.example.org");
  assert(iid4.get_local() == "");
  assert(iid4.get_server() == "irc.example.org");
  assert(!iid4.is_channel);
  assert(!iid4.is_user);

  return 0;
}
