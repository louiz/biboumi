#include "catch.hpp"

#include <bridge/colors.hpp>
#include <xmpp/xmpp_stanza.hpp>

#include <memory>

TEST_CASE("IRC colors parsing")
{
  std::unique_ptr<XmlNode> xhtml;
  std::string cleaned_up;

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("bold");
  CHECK(xhtml);
  CHECK(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'><span style='font-weight:bold;'>bold</span></body>");

  std::tie(cleaned_up, xhtml) =
    irc_format_to_xhtmlim("normalboldunder-and-boldbold normal"
                          "5red,5default-on-red10,2cyan-on-blue");
  CHECK(xhtml);
  CHECK(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>normal<span style='font-weight:bold;'>bold</span><span style='font-weight:bold;text-decoration:underline;'>under-and-bold</span><span style='font-weight:bold;'>bold</span> normal<span style='color:red;'>red</span><span style='background-color:red;'>default-on-red</span><span style='color:cyan;background-color:blue;'>cyan-on-blue</span></body>");
  CHECK(cleaned_up == "normalboldunder-and-boldbold normalreddefault-on-redcyan-on-blue");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("normal");
  CHECK_FALSE(xhtml);
  CHECK(cleaned_up == "normal");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("");
  CHECK(xhtml);
  CHECK(!xhtml->has_children());
  CHECK(cleaned_up.empty());

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim(",a");
  CHECK(xhtml);
  CHECK(!xhtml->has_children());
  CHECK(cleaned_up == "a");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim(",");
  CHECK(xhtml);
  CHECK(!xhtml->has_children());
  CHECK(cleaned_up.empty());

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("[\x1D13dolphin-emu/dolphin\x1D] 03foo commented on #283 (Add support for the guide button to XInput): 02http://example.com");
  CHECK(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>[<span style='font-style:italic;'/><span style='font-style:italic;color:lightmagenta;'>dolphin-emu/dolphin</span><span style='color:lightmagenta;'>] </span><span style='color:green;'>foo</span> commented on #283 (Add support for the guide button to XInput): <span style='text-decoration:underline;'/><span style='text-decoration:underline;color:blue;'>http://example.com</span><span style='text-decoration:underline;'/></body>");
  CHECK(cleaned_up == "[dolphin-emu/dolphin] foo commented on #283 (Add support for the guide button to XInput): http://example.com");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("0e46ab by 03Pierre Dindon [090|091|040] 02http://example.net/Ojrh4P media: avoid pop-in effect when loading thumbnails by specifying an explicit size");
  CHECK(cleaned_up == "0e46ab by Pierre Dindon [0|1|0] http://example.net/Ojrh4P media: avoid pop-in effect when loading thumbnails by specifying an explicit size");
  CHECK(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>0e46ab by <span style='color:green;'>Pierre Dindon</span> [<span style='color:lightgreen;'>0</span>|<span style='color:lightgreen;'>1</span>|<span style='color:indianred;'>0</span>] <span style='text-decoration:underline;'/><span style='text-decoration:underline;color:blue;'>http://example.net/Ojrh4P</span><span style='text-decoration:underline;'/> media: avoid pop-in effect when loading thumbnails by specifying an explicit size</body>");

  std::tie(cleaned_up, xhtml) = irc_format_to_xhtmlim("test\ncoucou");
  CHECK(cleaned_up == "test\ncoucou");
  CHECK(xhtml->to_string() == "<body xmlns='http://www.w3.org/1999/xhtml'>test<br/>coucou</body>");
}
