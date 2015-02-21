#ifndef LOGGER_INCLUDED
# define LOGGER_INCLUDED

/**
 * Singleton used in logger macros to write into files or stdout, with
 * various levels of severity.
 * Only the macros should be used.
 * @class Logger
 */

#include <memory>
#include <iostream>
#include <fstream>

#define debug_lvl 0
#define info_lvl 1
#define warning_lvl 2
#define error_lvl 3

#include "config.h"

#ifdef SYSTEMDDAEMON_FOUND
# include <systemd/sd-daemon.h>
#else
# define SD_DEBUG    "[DEBUG]: "
# define SD_INFO     "[INFO]: "
# define SD_WARNING  "[WARNING]: "
# define SD_ERR      "[ERROR]: "
#endif

// Macro defined to get the filename instead of the full path. But if it is
// not properly defined by the build system, we fallback to __FILE__
#ifndef __FILENAME__
# define __FILENAME__ __FILE__
#endif

#define WHERE\
  __FILENAME__ << ":" << __LINE__

#define log_debug(text)\
  Logger::instance()->get_stream(debug_lvl) << SD_DEBUG << WHERE << ":\t" << text << std::endl;

#define log_info(text)\
  Logger::instance()->get_stream(info_lvl) << SD_INFO << WHERE << ":\t" << text << std::endl;

#define log_warning(text)\
  Logger::instance()->get_stream(warning_lvl) << SD_WARNING << WHERE << ":\t" << text << std::endl;

#define log_error(text)\
  Logger::instance()->get_stream(error_lvl) << SD_ERR << WHERE << ":\t" << text << std::endl;

/**
 * Juste a structure representing a stream doing nothing with its input.
 */
class nullstream: public std::ostream
{
public:
  nullstream():
    std::ostream(0)
  { }
};

class Logger
{
public:
  static std::unique_ptr<Logger>& instance();
  std::ostream& get_stream(const int);
  Logger(const int log_level, const std::string& log_file);
  Logger(const int log_level);

private:
  Logger(const Logger&);
  Logger& operator=(const Logger&);

  const int log_level;
  std::ofstream ofstream;
  nullstream null_stream;
  std::ostream stream;
};

#endif // LOGGER_INCLUDED
