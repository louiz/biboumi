#include <config/config.hpp>

#include <iostream>
#include <sstream>

#include <stdlib.h>

std::string Config::filename = "./biboumi.cfg";
bool Config::file_must_exist = false;

std::string Config::get(const std::string& option, const std::string& def)
{
  Config* self = Config::instance().get();
  auto it = self->values.find(option);

  if (it == self->values.end())
    return def;
  return it->second;
}

int Config::get_int(const std::string& option, const int& def)
{
  Config* self = Config::instance().get();
  std::string res = self->get(option, "");
  if (!res.empty())
    return atoi(res.c_str());
  else
    return def;
}

void Config::set(const std::string& option, const std::string& value, bool save)
{
  Config* self = Config::instance().get();
  self->values[option] = value;
  if (save)
    {
      self->save_to_file();
      self->trigger_configuration_change();
    }
}

void Config::connect(t_config_changed_callback callback)
{
  Config* self = Config::instance().get();
  self->callbacks.push_back(callback);
}

void Config::close()
{
  Config* self = Config::instance().get();
  self->values.clear();
  Config::instance().reset();
}

/**
 * Private methods
 */

void Config::trigger_configuration_change()
{
  std::vector<t_config_changed_callback>::iterator it;
  for (it = this->callbacks.begin(); it < this->callbacks.end(); ++it)
      (*it)();
}

std::unique_ptr<Config>& Config::instance()
{
  static std::unique_ptr<Config> instance;

  if (!instance)
    {
      instance = std::make_unique<Config>();
      instance->read_conf();
    }
  return instance;
}

bool Config::read_conf()
{
  std::ifstream file;
  file.open(filename.data());
  if (!file.is_open())
    {
      if (Config::file_must_exist)
        {
          perror(("Error while opening file " + filename + " for reading.").c_str());
          file.exceptions(std::ifstream::failbit);
        }
      return false;
    }

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
      this->values[option] = value;
    }
  return true;
}

void Config::save_to_file() const
{
  std::ofstream file(this->filename.data());
  if (file.fail())
    {
      std::cerr << "Could not save config file." << std::endl;
      return ;
    }
  for (auto& it: this->values)
    file << it.first << "=" << it.second << std::endl;
  file.close();
}
