#include <config/config.hpp>
#include <utils/tolower.hpp>

#include <iostream>
#include <cstring>

#include <cstdlib>

using namespace std::string_literals;

extern char** environ;

std::string Config::filename{};
std::map<std::string, std::string> Config::values{};
std::vector<t_config_changed_callback> Config::callbacks{};

std::string Config::get(const std::string& option, const std::string& def)
{
  auto it = Config::values.find(option);

  if (it == Config::values.end())
    return def;
  return it->second;
}

bool Config::get_bool(const std::string& option, const bool def)
{
  auto res = Config::get(option, "");
  if (res.empty())
    return def;
  return res == "true";
}

int Config::get_int(const std::string& option, const int& def)
{
  std::string res = Config::get(option, "");
  if (!res.empty())
    return std::atoi(res.c_str());
  else
    return def;
}

bool Config::is_in_list(const std::string& option, const std::string& value)
{
  std::string res = Config::get(option, "");
  if (res.empty())
    return false;
  std::string::size_type n = 0;
  std::string::size_type length = value.size();
  while (true)
    {
      n = res.find(value, n);
      if (n == std::string::npos)
        break;
      if (n != 0 && res[n - 1] != ':')
        continue;
      if (res[n + length + 1] != '\0' && res[n + length + 1] != ':')
        continue;
      return true;
    }
  return false;
}

void Config::set(const std::string& option, const std::string& value, bool save)
{
  Config::values[option] = value;
  if (save)
    {
      Config::save_to_file();
      Config::trigger_configuration_change();
    }
}

void Config::connect(const t_config_changed_callback& callback)
{
    Config::callbacks.push_back(callback);
}

void Config::clear()
{
  Config::values.clear();
}

/**
 * Private methods
 */
void Config::trigger_configuration_change()
{
  std::vector<t_config_changed_callback>::iterator it;
  for (it = Config::callbacks.begin(); it < Config::callbacks.end(); ++it)
      (*it)();
}

bool Config::read_conf(const std::string& name)
{
  if (!name.empty())
    Config::filename = name;

  std::ifstream file(Config::filename.data());
  if (!file.is_open())
    {
      std::cerr << "Error while opening file " << filename << " for reading: " << strerror(errno) << std::endl;
      return false;
    }

  Config::clear();

  auto parse_line = [](const std::string& line, const bool env)
  {
    static const auto env_option_prefix = "BIBOUMI_"s;

    if (line == "" || line[0] == '#')
      return;
    size_t pos = line.find('=');
    if (pos == std::string::npos)
      return;
    std::string option = line.substr(0, pos);
    std::string value = line.substr(pos+1);
    if (env)
      {
        auto a = option.substr(0, env_option_prefix.size());
        if (a == env_option_prefix)
          option = utils::tolower(option.substr(env_option_prefix.size()));
        else
          return;
      }
    Config::values[option] = value;
  };

  std::string line;
  while (file.good())
    {
      std::getline(file, line);
      parse_line(line, false);
    }

  char** env_line = environ;
  while (*env_line)
    {
      parse_line(*env_line, true);
      env_line++;
    }
  return true;
}

void Config::save_to_file()
{
  std::ofstream file(Config::filename.data());
  if (file.fail())
    {
      std::cerr << "Could not save config file." << std::endl;
      return ;
    }
  for (const auto& it: Config::values)
    file << it.first << "=" << it.second << '\n';
}
