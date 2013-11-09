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
   * Convert the given string (encoded is "encoding") into valid utf-8.
   * If some decoding fails, insert an utf-8 placeholder character instead.
   */
  std::string convert_to_utf8(const std::string& str, const char* encoding);
}

#endif // ENCODING_INCLUDED
