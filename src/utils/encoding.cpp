#include <utils/encoding.hpp>

#include <utils/scopeguard.hpp>

#include <stdexcept>

#include <assert.h>
#include <string.h>
#include <iconv.h>

#include <config.h>

#include <bitset>

/**
 * The UTF-8-encoded character used as a place holder when a character conversion fails.
 * This is U+FFFD � "replacement character"
 */
static const char* invalid_char = "\xef\xbf\xbd";
static const size_t invalid_char_len = 3;

namespace utils
{
  /**
   * Based on http://en.wikipedia.org/wiki/UTF-8#Description
   */
  bool is_valid_utf8(const char* s)
  {
    if (!s)
      return false;

    const unsigned char* str = reinterpret_cast<const unsigned char*>(s);

    while (*str)
      {
        // 4 bytes:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((str[0] & 0b11111000) == 0b11110000)
          {
            if (!str[1] || !str[2] || !str[3]
                || ((str[1] & 0b11000000) != 0b10000000)
                || ((str[2] & 0b11000000) != 0b10000000)
                || ((str[3] & 0b11000000) != 0b10000000))
              return false;
            str += 4;
          }
        // 3 bytes:  1110xxx 10xxxxxx 10xxxxxx
        else if ((str[0] & 0b11110000) == 0b11100000)
          {
            if (!str[1] || !str[2]
                || ((str[1] & 0b11000000) != 0b10000000)
                || ((str[2] & 0b11000000) != 0b10000000))
              return false;
            str += 3;
          }
        // 2 bytes:  110xxxxx 10xxxxxx
        else if (((str[0]) & 0b11100000) == 0b11000000)
          {
            if (!str[1] ||
                ((str[1] & 0b11000000) != 0b10000000))
              return false;
            str += 2;
          }
        // 1 byte:  0xxxxxxx
        else if ((str[0] & 0b10000000) != 0)
          return false;
        else
          str++;
      }
    return true;
  }

  std::string remove_invalid_xml_chars(const std::string& original)
  {
    // The given string MUST be a valid utf-8 string
    unsigned char* res = new unsigned char[original.size()];
    ScopeGuard sg([&res]() { delete[] res;});

    // pointer where we write valid chars
    unsigned char* r = res;

    const unsigned char* str = reinterpret_cast<const unsigned char*>(original.c_str());
    std::bitset<20> codepoint;

    while (*str)
      {
        // 4 bytes:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        if ((str[0] & 0b11111000) == 0b11110000)
          {
            codepoint  = ((str[0] & 0b00000111) << 18);
            codepoint |= ((str[1] & 0b00111111) << 12);
            codepoint |= ((str[2] & 0b00111111) << 6 );
            codepoint |= ((str[3] & 0b00111111) << 0 );
            if (codepoint.to_ulong() <= 0x10FFFF)
              {
                ::memcpy(r, str, 4);
                r += 4;
              }
            str += 4;
          }
        // 3 bytes:  1110xxx 10xxxxxx 10xxxxxx
        else if ((str[0] & 0b11110000) == 0b11100000)
          {
            codepoint  = ((str[0] & 0b00001111) << 12);
            codepoint |= ((str[1] & 0b00111111) << 6);
            codepoint |= ((str[2] & 0b00111111) << 0 );
            if (codepoint.to_ulong() <= 0xD7FF ||
                (codepoint.to_ulong() >= 0xE000 && codepoint.to_ulong() <= 0xFFFD))
              {
                ::memcpy(r, str, 3);
                r += 3;
              }
            str += 3;
          }
        // 2 bytes:  110xxxxx 10xxxxxx
        else if (((str[0]) & 0b11100000) == 0b11000000)
          {
            // All 2 bytes char are valid, don't even bother calculating
            // the codepoint
            ::memcpy(r, str, 2);
            r += 2;
            str += 2;
          }
        // 1 byte:  0xxxxxxx
        else if ((str[0] & 0b10000000) == 0)
          {
            codepoint = ((str[0] & 0b01111111));
            if (codepoint.to_ulong() == 0x09 ||
                codepoint.to_ulong() == 0x0A ||
                codepoint.to_ulong() == 0x0D ||
                codepoint.to_ulong() >= 0x20)
              {
                ::memcpy(r, str, 1);
                r += 1;
              }
            str += 1;
          }
        else
          throw std::runtime_error("Invalid UTF-8 passed to remove_invalid_xml_chars");
      }
    return std::string(reinterpret_cast<char*>(res), r-res);
  }

  std::string convert_to_utf8(const std::string& str, const char* charset)
  {
    std::string res;

    const iconv_t cd = iconv_open("UTF-8", charset);
    if (cd == (iconv_t)-1)
      throw std::runtime_error("Cannot convert into UTF-8");

    // Make sure cd is always closed when we leave this function
    ScopeGuard sg([&]{ iconv_close(cd); });

    size_t inbytesleft = str.size();

    // iconv will not attempt to modify this buffer, but some plateform
    // require a char** anyway
#ifdef ICONV_SECOND_ARGUMENT_IS_CONST
    const char* inbuf_ptr = str.c_str();
#else
    char* inbuf_ptr = const_cast<char*>(str.c_str());
#endif

    size_t outbytesleft = str.size() * 4;
    char* outbuf = new char[outbytesleft];
    char* outbuf_ptr = outbuf;

    // Make sure outbuf is always deleted when we leave this function
    sg.add_callback([&]{ delete[] outbuf; });

    bool done = false;
    while (done == false)
      {
        size_t error = iconv(cd, &inbuf_ptr, &inbytesleft, &outbuf_ptr, &outbytesleft);
        if ((size_t)-1 == error)
          {
            switch (errno)
              {
              case EILSEQ:
                // Invalid byte found. Insert a placeholder instead of the
                // converted character, jump one byte and continue
                memcpy(outbuf_ptr, invalid_char, invalid_char_len);
                outbuf_ptr += invalid_char_len;
                inbytesleft--;
                inbuf_ptr++;
                break;
              case EINVAL:
                // A multibyte sequence is not terminated, but we can't
                // provide any more data, so we just add a placeholder to
                // indicate that the character is not properly converted,
                // and we stop the conversion
                memcpy(outbuf_ptr, invalid_char, invalid_char_len);
                outbuf_ptr += invalid_char_len;
                outbuf_ptr++;
                done = true;
                break;
              case E2BIG:
                // This should never happen
                done = true;
                break;
              default:
                // This should happen even neverer
                done = true;
                break;
              }
          }
        else
          {
            // The conversion finished without any error, stop converting
            done = true;
          }
      }
    // Terminate the converted buffer, and copy that buffer it into the
    // string we return
    *outbuf_ptr = '\0';
    res = outbuf;
    return res;
  }

}

