/**
 * Read the config file and save all the values in a map.
 * Also, a singleton.
 *
 * Use Config::filename = "bla" to set the filename you want to use.
 *
 * If you want to exit if the file does not exist when it is open for
 * reading, set Config::file_must_exist = true.
 *
 * Config::get() can the be used to access the values in the conf.
 *
 * Use Config::close() when you're done getting/setting value. This will
 * save the config into the file.
 */

#ifndef CONFIG_INCLUDED
# define CONFIG_INCLUDED

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
  Config(){};
  ~Config(){};
  /**
   * returns a value from the config. If it doesn’t exist, use
   * the second argument as the default.
   * @param option The option we want
   * @param def The default value in case the option does not exist
   */
  static std::string get(const std::string&, const std::string&);
  /**
   * returns a value from the config. If it doesn’t exist, use
   * the second argument as the default.
   * @param option The option we want
   * @param def The default value in case the option does not exist
   */
  static int get_int(const std::string&, const int&);
  /**
   * Set a value for the given option. And write all the config
   * in the file from which it was read if boolean is set.
   * @param option The option to set
   * @param value The value to use
   * @param save if true, save the config file
   */
  static void set(const std::string&, const std::string&, bool save = false);
  /**
   * Adds a function to a list. This function will be called whenever a
   * configuration change occurs.
   */
  static void connect(t_config_changed_callback);
  /**
   * Close the config file, saving it to the file is save == true.
   */
  static void close();

  /**
   * Set the value of the filename to use, before calling any method.
   */
  static std::string filename;
  /**
   * Set to true if you want an exception to be raised if the file does not
   * exist when reading it.
   */
  static bool file_must_exist;

private:
  /**
   * Get the singleton instance
   */
  static std::unique_ptr<Config>& instance();
  /**
   * Read the configuration file at the given path.
   */
  bool read_conf();
  /**
   * Write all the config values into the configuration file
   */
  void save_to_file() const;
  /**
   * Call all the callbacks previously registered using connect().
   * This is used to notify any class that a configuration change occured.
   */
  void trigger_configuration_change();

  std::map<std::string, std::string> values;
  std::vector<t_config_changed_callback> callbacks;

  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;
  Config(Config&&) = delete;
  Config& operator=(Config&&) = delete;
};

#endif // CONFIG_INCLUDED
