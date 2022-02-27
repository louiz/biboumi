#include "catch2/catch.hpp"

#include <xmpp/xmpp_parser.hpp>
#include <xmpp/auth.hpp>

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
        // And do the same checks on moved-constructed stanza
        Stanza moved(std::move(copy));
      });
  CHECK(doc.size() <= std::numeric_limits<int>::max());
  xml.feed(doc.data(), static_cast<int>(doc.size()), true);

  const std::string doc2 = "<stream xmlns='s'><stanza>coucou\r\n\a</stanza></stream>";
  xml.add_stanza_callback([](const Stanza& stanza)
      {
        CHECK(stanza.get_inner() == "coucou\r\n");
      });
  CHECK(doc.size() <= std::numeric_limits<int>::max());
  xml.feed(doc2.data(), static_cast<int>(doc.size()), true);
}

TEST_CASE("XML escape")
{
  const std::string unescaped = R"('coucou'<cc>/&"gaga")";
  CHECK(xml_escape(unescaped) == "&apos;coucou&apos;&lt;cc&gt;/&amp;&quot;gaga&quot;");
}

TEST_CASE("handshake_digest")
{
  const auto res = get_handshake_digest("id1234", "S4CR3T");
  CHECK(res == "c92901b5d376ad56269914da0cce3aab976847df");
}

TEST_CASE("substanzas")
{
  Stanza a("a");
  {
    XmlSubNode b(a, "b");
    {
      CHECK(!a.has_children());
      XmlSubNode c(b, "c");
      XmlSubNode d(b, "d");
      CHECK(!c.has_children());
      CHECK(!d.has_children());
    }
    CHECK(b.has_children());
  }
  CHECK(a.has_children());
}
