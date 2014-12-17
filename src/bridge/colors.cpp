#include <bridge/colors.hpp>
#include <xmpp/xmpp_stanza.hpp>

#include <algorithm>
#include <iostream>

#include <string.h>

using namespace std::string_literals;

static const char IRC_NUM_COLORS = 16;

static const char* irc_colors_to_css[IRC_NUM_COLORS] = {
  "white",
  "black",
  "blue",
  "green",
  "indianred",
  "red",
  "magenta",
  "brown",
  "yellow",
  "lightgreen",
  "cyan",
  "lightcyan",
  "lightblue",
  "lightmagenta",
  "gray",
  "white",
};

#define XHTML_NS "http://www.w3.org/1999/xhtml"

struct styles_t
{
  bool strong;
  bool underline;
  bool italic;
  int fg;
  int bg;
};

/** We keep the currently-applied CSS styles in a structure. Each time a tag
 * is found, update this style list, then close the current span XML element
 * (if it is open), then reopen it with all the new styles in it.  This is
 * done this way because IRC formatting does not map well with XML
 * (hierarchical tags), it’s a lot easier and cleaner to remove all styles
 * and reapply them for each tag, instead of trying to keep a consistent
 * hierarchy of span, strong, em etc tags.  The generated XML is one-level
 * deep only.
*/
Xmpp::body irc_format_to_xhtmlim(const std::string& s)
{
  if (s.find_first_of(irc_format_char) == std::string::npos)
    // there is no special formatting at all
    return std::make_tuple(s, nullptr);

  std::string cleaned;

  styles_t styles = {false, false, false, -1, -1};

  std::unique_ptr<XmlNode> result = std::make_unique<XmlNode>("body");
  (*result)["xmlns"] = XHTML_NS;

  XmlNode* current_node = result.get();
  std::string::size_type pos_start = 0;
  std::string::size_type pos_end;

  while ((pos_end = s.find_first_of(irc_format_char, pos_start)) != std::string::npos)
    {
      const std::string txt = s.substr(pos_start, pos_end-pos_start);
      cleaned += txt;
      if (current_node->has_children())
        current_node->get_last_child()->add_to_tail(txt);
      else
        current_node->add_to_inner(txt);

      if (s[pos_end] == IRC_FORMAT_BOLD_CHAR)
        styles.strong = !styles.strong;
      else if (s[pos_end] == IRC_FORMAT_NEWLINE_CHAR)
        {
          XmlNode* br_node = new XmlNode("br");
          br_node->close();
          current_node->add_child(br_node);
          cleaned += '\n';
        }
      else if (s[pos_end] == IRC_FORMAT_UNDERLINE_CHAR)
        styles.underline = !styles.underline;
      else if (s[pos_end] == IRC_FORMAT_ITALIC_CHAR)
        styles.italic = !styles.italic;
      else if (s[pos_end] == IRC_FORMAT_RESET_CHAR)
        styles = {false, false, false, -1, -1};
      else if (s[pos_end] == IRC_FORMAT_REVERSE_CHAR)
        { }                      // TODO
      else if (s[pos_end] == IRC_FORMAT_REVERSE2_CHAR)
        { }                      // TODO
      else if (s[pos_end] == IRC_FORMAT_FIXED_CHAR)
        { }                      // TODO
      else if (s[pos_end] == IRC_FORMAT_COLOR_CHAR)
        {
          size_t pos = pos_end + 1;
          styles.fg = -1;
          styles.bg = -1;
          // get the first number following the format char
          if (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
            {                   // first digit
              styles.fg = s[pos++] - '0';
              if (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
                // second digit
                styles.fg = styles.fg * 10 + s[pos++] - '0';
            }
          if (pos < s.size() && s[pos] == ',')
            {                   // get bg color after the comma
              pos++;
              if (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
                {               // first digit
                  styles.bg = s[pos++] - '0';
                  if (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
                    // second digit
                    styles.bg = styles.bg * 10 + s[pos++] - '0';
                }
            }
          pos_end = pos - 1;
        }

      // close opened span, if any
      if (current_node != result.get())
        {
          current_node->close();
          result->add_child(current_node);
          current_node = result.get();
        }
      // Take all currently-applied style and create a new span with it
      std::string styles_str;
      if (styles.strong)
        styles_str += "font-weight:bold;";
      if (styles.underline)
        styles_str += "text-decoration:underline;";
      if (styles.italic)
        styles_str += "font-style:italic;";
      if (styles.fg != -1)
        styles_str += "color:"s +
          irc_colors_to_css[styles.fg % IRC_NUM_COLORS] + ";";
      if (styles.bg != -1)
        styles_str += "background-color:"s +
          irc_colors_to_css[styles.bg % IRC_NUM_COLORS] + ";";
      if (!styles_str.empty())
        {
          current_node = new XmlNode("span");
          (*current_node)["style"] = styles_str;
        }

      pos_start = pos_end + 1;
    }

  // If some text remains, without any format char, just append that text at
  // the end of the current node
  const std::string txt = s.substr(pos_start, pos_end-pos_start);
  cleaned += txt;
  if (current_node->has_children())
    current_node->get_last_child()->add_to_tail(txt);
  else
    current_node->add_to_inner(txt);

  if (current_node != result.get())
    {
      current_node->close();
      result->add_child(current_node);
    }

  result->close();
  Xmpp::body body_res = std::make_tuple(cleaned, std::move(result));
  return body_res;
}
