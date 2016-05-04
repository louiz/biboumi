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
  static void connect(t_config_changed_callback);
  /**
   * Destroy the instance, forcing it to be recreated (with potentially
   * different parameters) the next time it’s needed.
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

};

#endif // CONFIG_INCLUDED
