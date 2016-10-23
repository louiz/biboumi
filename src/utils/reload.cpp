#include <utils/reload.hpp>
#include <database/database.hpp>
#include <config/config.hpp>
#include <utils/xdg.hpp>
#include <logger/logger.hpp>

#include "biboumi.h"

void open_database()
{
#ifdef USE_DATABASE
  const auto db_filename = Config::get("db_name", xdg_data_path("biboumi.sqlite"));
  log_info("Opening database: ", db_filename);
  Database::open(db_filename);
  log_info("database successfully opened.");
#endif
}

void reload_process()
{
  Config::read_conf();
  // Destroy the logger instance, to be recreated the next time a log
  // line needs to be written
  Logger::instance().reset();
  log_info("Configuration and logger reloaded.");
#ifdef USE_DATABASE
  try {
      open_database();
    } catch (const litesql::DatabaseError&) {
      log_warning("Re-using the previous database.");
    }
#endif
}

