#ifndef COLORS_INCLUDED
# define COLORS_INCLUDED

/**
 * A module handling the conversion between IRC colors and XHTML-IM, and
 * vice versa.
 */

#include <string>
#include <memory>
#include <tuple>

class XmlNode;

namespace Xmpp
{
// Contains:
// - an XMPP-valid UTF-8 body
// - an XML node representing the XHTML-IM body, or null
  typedef std::tuple<const std::string, std::unique_ptr<XmlNode>> body;
}

#define IRC_FORMAT_BOLD_CHAR       '\x02' // done
#define IRC_FORMAT_COLOR_CHAR      '\x03' // done
#define IRC_FORMAT_RESET_CHAR      '\x0F' // done
#define IRC_FORMAT_FIXED_CHAR      '\x11' // ??
#define IRC_FORMAT_REVERSE_CHAR    '\x12' // maybe one day
#define IRC_FORMAT_REVERSE2_CHAR   '\x16' // wat
#define IRC_FORMAT_ITALIC_CHAR     '\x1D' // done
#define IRC_FORMAT_UNDERLINE_CHAR  '\x1F' // done
#define IRC_FORMAT_NEWLINE_CHAR    '\n'   // done

static const char irc_format_char[] = {
  IRC_FORMAT_BOLD_CHAR,
  IRC_FORMAT_COLOR_CHAR,
  IRC_FORMAT_RESET_CHAR,
  IRC_FORMAT_FIXED_CHAR,
  IRC_FORMAT_REVERSE_CHAR,
  IRC_FORMAT_REVERSE2_CHAR,
  IRC_FORMAT_ITALIC_CHAR,
  IRC_FORMAT_UNDERLINE_CHAR,
  IRC_FORMAT_NEWLINE_CHAR,
  '\x00'
};

/**
 * Convert the passed string into an XML tree representing the XHTML version
 * of the message, converting the IRC colors symbols into xhtml-im
 * formatting.
 *
 * Returns the body cleaned from any IRC formatting (but without any xhtml),
 * and the body as XHTML-IM
 */
Xmpp::body irc_format_to_xhtmlim(const std::string& str);

#endif // COLORS_INCLUDED
