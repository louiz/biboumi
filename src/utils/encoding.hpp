#ifndef ENCODING_INCLUDED
# define ENCODING_INCLUDED

#include <string>

namespace utils
{
  /**
   * Returns true if the given null-terminated string is valid utf-8.
   *
   * Based on http://en.wikipedia.org/wiki/UTF-8#Description
   */
  bool is_valid_utf8(const char* s);
  /**
   * Remove all invalid codepoints from the given utf-8-encoded string.
   * The value returned is a copy of the string, without the removed chars.
   *
   * See http://www.w3.org/TR/xml/#charsets for the list of valid characters
   * in XML.
   */
  std::string remove_invalid_xml_chars(const std::string& original);
  /**
   * Convert the given string (encoded is "encoding") into valid utf-8.
   * If some decoding fails, insert an utf-8 placeholder character instead.
   */
  std::string convert_to_utf8(const std::string& str, const char* encoding);
}

#endif // ENCODING_INCLUDED
