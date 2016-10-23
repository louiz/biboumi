#include <utils/string.hpp>
#include <utils/encoding.hpp>

bool to_bool(const std::string& val)
{
  return (val == "1" || val == "true");
}

std::vector<std::string> cut(const std::string& val, const std::size_t size)
{
  std::vector<std::string> res;
  std::string::size_type pos = 0;
  while (pos < val.size())
    {
      // Get the number of chars, <= size, that contain only whole
      // UTF-8 codepoints.
      std::size_t s = 0;
      auto codepoint_size = utils::get_next_codepoint_size(val[pos + s]);
      while (s + codepoint_size <= size && pos + s < val.size())
        {
          s += codepoint_size;
          codepoint_size = utils::get_next_codepoint_size(val[pos + s]);
        }
      res.emplace_back(val.substr(pos, s));
      pos += s;
    }
  return res;
}
