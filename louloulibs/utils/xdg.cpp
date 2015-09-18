#include <utils/xdg.hpp>
#include <cstdlib>

#include "louloulibs.h"

std::string xdg_config_path(const std::string& filename)
{
  const char* xdg_config_home = ::getenv("XDG_CONFIG_HOME");
  if (xdg_config_home && xdg_config_home[0] == '/')
    return std::string{xdg_config_home} + "/" PROJECT_NAME "/" + filename;
  else
    {
      const char* home = ::getenv("HOME");
      if (home)
        return std::string{home} + "/" ".config" "/" PROJECT_NAME "/" + filename;
      else
        return filename;
    }
}

