#pragma once


/**
 * Singleton used in logger macros to write into files or stdout, with
 * various levels of severity.
 * Only the macros should be used.
 * @class Logger
 */

#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#define debug_lvl 0
#define info_lvl 1
#define warning_lvl 2
#define error_lvl 3

#include "biboumi.h"
#ifdef SYSTEMD_FOUND
#define SD_JOURNAL_SUPPRESS_LOCATION
# include <systemd/sd-daemon.h>
# include <systemd/sd-journal.h>
#else
# define SD_DEBUG    "[DEBUG]: "
# define SD_INFO     "[INFO]: "
# define SD_WARNING  "[WARNING]: "
# define SD_ERR      "[ERROR]: "
# define LOG_ERR 3
# define LOG_WARNING 4
# define LOG_INFO 6
# define LOG_DEBUG 7
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

#ifdef SYSTEMD_FOUND
  bool use_stdout() const
  {
    return this->stream.rdbuf() == std::cout.rdbuf();
  }

  bool use_systemd{false};
#endif

private:
  const int log_level;
  std::ofstream ofstream{};
  std::ostream stream;

  NullBuffer null_buffer;
  std::ostream null_stream;
};

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
  void do_logging(const int level, int syslog_level, const char* src_file, int line, U&&... args)
  {
  #ifdef SYSTEMD_FOUND
    if (Logger::instance()->use_systemd)
      {
        (void)level;
        std::ostringstream os;
        log(os, std::forward<U>(args)...);
        sd_journal_send("MESSAGE=%s", os.str().data(),
                        "PRIORITY=%i", syslog_level,
                        "CODE_FILE=%s", src_file,
                        "CODE_LINE=%i", line,
                        nullptr);
      }
    else
      {
  #endif
        (void)syslog_level;
        static const char* priority_names[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
        auto& os = Logger::instance()->get_stream(level);
        os << '[' << priority_names[level] << "]: " << src_file << ':' << line << ":\t";
        log(os, std::forward<U>(args)...);
#ifdef SYSTEMD_FOUND
      }
#endif
  }
}

#define log_debug(...) logging_details::do_logging(debug_lvl, LOG_DEBUG, __FILENAME__, __LINE__, __VA_ARGS__)

#define log_info(...) logging_details::do_logging(info_lvl, LOG_INFO, __FILENAME__, __LINE__, __VA_ARGS__)

#define log_warning(...) logging_details::do_logging(warning_lvl, LOG_WARNING, __FILENAME__, __LINE__, __VA_ARGS__)

#define log_error(...) logging_details::do_logging(error_lvl, LOG_ERR, __FILENAME__, __LINE__, __VA_ARGS__)
