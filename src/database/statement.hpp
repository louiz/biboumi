#pragma once

#include <sqlite3.h>

class Statement
{
 public:
  Statement(sqlite3_stmt* stmt):
      stmt(stmt) {}
  ~Statement()
  {
    sqlite3_finalize(this->stmt);
  }

  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;
  Statement(Statement&& other):
      stmt(other.stmt)
  {
    other.stmt = nullptr;
  }
  Statement& operator=(Statement&& other)
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
};
