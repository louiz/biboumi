#include <config/config.hpp>
#include <logger/logger.hpp>

void reload_process()
{
  Config::read_conf();
  // Destroy the logger instance, to be recreated the next time a log
  // line needs to be written
  Logger::instance().reset();
  log_debug("Configuration and logger reloaded.");
}
