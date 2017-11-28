#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class StepResult
{
  Row,
  Done,
  Error,
};

class Statement
{
 public:
  virtual ~Statement() = default;
  virtual StepResult step() = 0;

  virtual void bind(std::vector<std::string> params) = 0;

  virtual std::int64_t get_column_int64(const int col) = 0;
  virtual std::string get_column_text(const int col) = 0;
  virtual int get_column_int(const int col) = 0;

  virtual bool bind_text(const int pos, const std::string& data) = 0;
  virtual bool bind_int64(const int pos, const std::int64_t value) = 0;
  virtual bool bind_null(const int pos) = 0;
};
