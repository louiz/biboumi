#include <utils/dirname.hpp>

namespace utils
{
    std::string dirname(const std::string& filename)
    {
      if (filename.empty())
        return "./";
      if (filename == ".." || filename == ".")
        return filename;
      auto pos = filename.rfind('/');
      if (pos == std::string::npos)
        return "./";
      return filename.substr(0, pos + 1);
    }
}
