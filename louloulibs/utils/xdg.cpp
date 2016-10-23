#include <utils/xdg.hpp>
#include <cstdlib>

#include "louloulibs.h"

std::string xdg_path(const std::string& filename, const char* env_var)
{
  const char* xdg_home = ::getenv(env_var);
  if (xdg_home && xdg_home[0] == '/')
    return std::string{xdg_home} + "/" PROJECT_NAME "/" + filename;
  else
    {
      const char* home = ::getenv("HOME");
      if (home)
        return std::string{home} + "/" ".config" "/" PROJECT_NAME "/" + filename;
      else
        return filename;
    }
}

std::string xdg_config_path(const std::string& filename)
{
  return xdg_path(filename, "XDG_CONFIG_HOME");
}

std::string xdg_data_path(const std::string& filename)
{
  return xdg_path(filename, "XDG_DATA_HOME");
}
