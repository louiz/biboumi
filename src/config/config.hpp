/**
 * Read the config file and save all the values in a map.
 * Also, a singleton.
 *
 * Use Config::filename = "bla" to set the filename you want to use.
 *
 * If you want to exit if the file does not exist when it is open for
 * reading, set Config::file_must_exist = true.
 *
 * Config::get() can then be used to access the values in the conf.
 *
 * Use Config::close() when you're done getting/setting value. This will
 * save the config into the file.
 */

#pragma once

#include <functional>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <map>

typedef std::function<void()> t_config_changed_callback;

class Config
{
public:
  Config() = default;
  ~Config() = default;
  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;
  Config(Config&&) = delete;
  Config& operator=(Config&&) = delete;

  /**
   * returns a value from the config. If it doesn’t exist, use
   * the second argument as the default.
   */
  static std::string get(const std::string&, const std::string&);
  /**
   * returns a value from the config. If it doesn’t exist, use
   * the second argument as the default.
   */
  static int get_int(const std::string&, const int&);
  /**
   * Set a value for the given option. And write all the config
   * in the file from which it was read if save is true.
   */
  static void set(const std::string&, const std::string&, bool save = false);
  /**
   * Adds a function to a list. This function will be called whenever a
   * configuration change occurs (when set() is called, or when the initial
   * conf is read)
   */
  static void connect(const t_config_changed_callback&);
  /**
   * Destroy the instance, forcing it to be recreated (with potentially
   * different parameters) the next time it’s needed.
   */
  static void clear();
  /**
   * Read the configuration file at the given path.
   */
  static bool read_conf(const std::string& name="");
  /**
   * Get the filename
   */
  static const std::string& get_filename()
  { return Config::filename; }

private:
  /**
   * Set the value of the filename to use, before calling any method.
   */
  static std::string filename;
  /**
   * Write all the config values into the configuration file
   */
  static void save_to_file();
  /**
   * Call all the callbacks previously registered using connect().
   * This is used to notify any class that a configuration change occured.
   */
  static void trigger_configuration_change();

  static std::map<std::string, std::string> values;
  static std::vector<t_config_changed_callback> callbacks;

};


