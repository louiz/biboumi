#pragma once

#include <database/statement.hpp>

#include <logger/logger.hpp>

#include <sqlite3.h>

class Sqlite3Statement: public Statement
{
 public:
  Sqlite3Statement(sqlite3_stmt* stmt):
      stmt(stmt) {}
  virtual ~Sqlite3Statement()
  {
    sqlite3_finalize(this->stmt);
  }

  StepResult step() override final
  {
    auto res = sqlite3_step(this->get());
    log_debug("step: ", res);
    if (res == SQLITE_ROW)
      return StepResult::Row;
    else if (res == SQLITE_DONE)
      return StepResult::Done;
    else
      return StepResult::Error;
  }

  void bind(std::vector<std::string> params) override
  {
  int i = 1;
  for (const std::string& param: params)
    {
      if (sqlite3_bind_text(this->get(), i, param.data(), static_cast<int>(param.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        log_error("Failed to bind ", param, " to param ", i);
      i++;
    }
  }

  int64_t get_column_int64(const int col) override
  {
    return sqlite3_column_int64(this->get(), col);
  }

  std::string get_column_text(const int col) override
  {
    const auto size = sqlite3_column_bytes(this->get(), col);
    const unsigned char* str = sqlite3_column_text(this->get(), col);
    std::string result(reinterpret_cast<const char*>(str), static_cast<std::size_t>(size));
    return result;
  }

  bool bind_text(const int pos, const std::string& data) override
  {
    return sqlite3_bind_text(this->get(), pos, data.data(), static_cast<int>(data.size()), SQLITE_TRANSIENT) == SQLITE_OK;
  }
  bool bind_int64(const int pos, const std::int64_t value) override
  {
    return sqlite3_bind_int64(this->get(), pos, static_cast<sqlite3_int64>(value)) == SQLITE_OK;
  }
  bool bind_null(const int pos) override
  {
    return sqlite3_bind_null(this->get(), pos) == SQLITE_OK;
  }
  int get_column_int(const int col) override
  {
    return sqlite3_column_int(this->get(), col);
  }

  Sqlite3Statement(const Sqlite3Statement&) = delete;
  Sqlite3Statement& operator=(const Sqlite3Statement&) = delete;
  Sqlite3Statement(Sqlite3Statement&& other):
      stmt(other.stmt)
  {
    other.stmt = nullptr;
  }
  Sqlite3Statement& operator=(Sqlite3Statement&& other)
  {
    this->stmt = other.stmt;
    other.stmt = nullptr;
    return *this;
  }
  sqlite3_stmt* get()
  {
    return this->stmt;
  }

 private:
  sqlite3_stmt* stmt;
  int last_step_result{SQLITE_OK};
};
