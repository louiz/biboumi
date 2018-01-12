#include <logger/logger.hpp>
#include <config/config.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

Logger::Logger(const int log_level):
  log_level(log_level),
  stream(std::cout.rdbuf()),
  null_buffer{},
  null_stream{&null_buffer}
{
#ifdef SYSTEMD_FOUND
  if (!this->use_stdout())
    return;

  // See https://www.freedesktop.org/software/systemd/man/systemd.exec.html#%24JOURNAL_STREAM
  const char* journal_stream = ::getenv("JOURNAL_STREAM");
  if (journal_stream == nullptr)
    return;

  struct stat s{};
  const int res = ::fstat(STDOUT_FILENO, &s);
  if (res == -1)
    return;

  const auto stdout_stream = std::to_string(s.st_dev) + ":" + std::to_string(s.st_ino);

  if (stdout_stream == journal_stream)
    this->use_systemd = true;
#endif
}

Logger::Logger(const int log_level, const std::string& log_file):
  log_level(log_level),
  ofstream(log_file.data(), std::ios_base::app),
  stream(ofstream.rdbuf()),
  null_buffer{},
  null_stream{&null_buffer}
{
}

std::unique_ptr<Logger>& Logger::instance()
{
  static std::unique_ptr<Logger> instance;

  if (!instance)
    {
      const std::string log_file = Config::get("log_file", "");
      const int log_level = Config::get_int("log_level", 0);
      if (log_file.empty())
        instance = std::make_unique<Logger>(log_level);
      else
        instance = std::make_unique<Logger>(log_level, log_file);
    }
  return instance;
}

std::ostream& Logger::get_stream(const int lvl)
{
  if (lvl >= this->log_level)
    return this->stream;
  return this->null_stream;
}
