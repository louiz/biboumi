#include <config/config.hpp>

#include <iostream>
#include <cstring>

#include <cstdlib>

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

int Config::get_int(const std::string& option, const int& def)
{
  std::string res = Config::get(option, "");
  if (!res.empty())
    return std::atoi(res.c_str());
  else
    return def;
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

void Config::connect(t_config_changed_callback callback)
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

  std::string line;
  size_t pos;
  std::string option;
  std::string value;
  while (file.good())
    {
      std::getline(file, line);
      if (line == "" || line[0] == '#')
        continue ;
      pos = line.find('=');
      if (pos == std::string::npos)
        continue ;
      option = line.substr(0, pos);
      value = line.substr(pos+1);
      Config::values[option] = value;
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
