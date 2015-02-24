#include <config/config.hpp>
#include <logger/logger.hpp>

void reload_process()
{
  // Closing the config will just force it to be reopened the next time
  // a configuration option is needed
  Config::close();
  // Destroy the logger instance, to be recreated the next time a log
  // line needs to be written
  Logger::instance().reset();
  log_debug("Configuration and logger reloaded.");
}
