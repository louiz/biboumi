#pragma once


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

#include "biboumi.h"
#ifdef SYSTEMD_FOUND
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


/**
 * A buffer, used to construct an ostream that does nothing
 * when we output data in it
 */
class NullBuffer: public std::streambuf
{
 public:
  int overflow(int c) { return c; }
};

class Logger
{
public:
  static std::unique_ptr<Logger>& instance();
  std::ostream& get_stream(const int);
  Logger(const int log_level, const std::string& log_file);
  Logger(const int log_level);

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&) = delete;
  Logger& operator=(Logger&&) = delete;

private:
  const int log_level;
  std::ofstream ofstream{};
  std::ostream stream;

  NullBuffer null_buffer;
  std::ostream null_stream;
};

#define WHERE __FILENAME__, ":", __LINE__, ":\t"

namespace logging_details
{
  template <typename T>
  void log(std::ostream& os, const T& arg)
  {
    os << arg << std::endl;
  }

  template <typename T, typename... U>
  void log(std::ostream& os, const T& first, U&&... rest)
  {
    os << first;
    log(os, std::forward<U>(rest)...);
  }

  template <typename... U>
  void log_debug(U&&... args)
  {
    auto& os = Logger::instance()->get_stream(debug_lvl);
    os << SD_DEBUG;
    log(os, std::forward<U>(args)...);
  }

  template <typename... U>
  void log_info(U&&... args)
  {
    auto& os = Logger::instance()->get_stream(info_lvl);
    os << SD_INFO;
    log(os, std::forward<U>(args)...);
  }

  template <typename... U>
  void log_warning(U&&... args)
  {
    auto& os = Logger::instance()->get_stream(warning_lvl);
    os << SD_WARNING;
    log(os, std::forward<U>(args)...);
  }

  template <typename... U>
  void log_error(U&&... args)
  {
    auto& os = Logger::instance()->get_stream(error_lvl);
    os << SD_ERR;
    log(os, std::forward<U>(args)...);
  }
}

#define log_info(...) logging_details::log_info(WHERE, __VA_ARGS__)

#define log_warning(...) logging_details::log_warning(WHERE, __VA_ARGS__)

#define log_error(...) logging_details::log_error(WHERE, __VA_ARGS__)

#define log_debug(...) logging_details::log_debug(WHERE, __VA_ARGS__)



