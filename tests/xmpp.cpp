#include "catch.hpp"

#include <xmpp/xmpp_parser.hpp>

TEST_CASE("Test basic XML parsing")
{
  XmppParser xml;

  const std::string doc = "<stream xmlns='stream_ns'><stanza b='c'>inner<child1><grandchild/></child1><child2 xmlns='child2_ns'/>tail</stanza></stream>";

  auto check_stanza = [](const Stanza& stanza)
    {
      CHECK(stanza.get_name() == "stanza");
      CHECK(stanza.get_tag("xmlns") == "stream_ns");
      CHECK(stanza.get_tag("b") == "c");
      CHECK(stanza.get_inner() == "inner");
      CHECK(stanza.get_tail() == "");
      CHECK(stanza.get_child("child1", "stream_ns") != nullptr);
      CHECK(stanza.get_child("child2", "stream_ns") == nullptr);
      CHECK(stanza.get_child("child2", "child2_ns") != nullptr);
      CHECK(stanza.get_child("child2", "child2_ns")->get_tail() == "tail");
    };
  xml.add_stanza_callback([check_stanza](const Stanza& stanza)
      {
        check_stanza(stanza);
        // Do the same checks on a copy of that stanza.
        Stanza copy(stanza);
        check_stanza(copy);
      });
  xml.feed(doc.data(), doc.size(), true);

  const std::string doc2 = "<stream xmlns='s'><stanza>coucou\r\n\a</stanza></stream>";
  xml.add_stanza_callback([](const Stanza& stanza)
      {
        CHECK(stanza.get_inner() == "coucou\r\n");
      });

  xml.feed(doc2.data(), doc.size(), true);
}

TEST_CASE("XML escape/unescape")
{
  const std::string unescaped = "'coucou'<cc>/&\"gaga\"";
  CHECK(xml_escape(unescaped) == "&apos;coucou&apos;&lt;cc&gt;/&amp;&quot;gaga&quot;");
  CHECK(xml_unescape(xml_escape(unescaped)) == unescaped);
}

