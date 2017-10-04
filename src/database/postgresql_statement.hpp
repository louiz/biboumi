#pragma once

#include <database/statement.hpp>

#include <logger/logger.hpp>

#include <libpq-fe.h>

class PostgresqlStatement: public Statement
{
 public:
  PostgresqlStatement(std::string body, PGconn*const conn):
      body(std::move(body)),
      conn(conn)
  {}
  ~PostgresqlStatement()
  {}
  PostgresqlStatement(const PostgresqlStatement&) = delete;
  PostgresqlStatement& operator=(const PostgresqlStatement&) = delete;
  PostgresqlStatement(PostgresqlStatement&& other) = delete;
  PostgresqlStatement& operator=(PostgresqlStatement&& other) = delete;

  StepResult step() override final
  {
    if (!this->executed)
      {
        this->current_tuple = 0;
        this->executed = true;
        if (!this->execute())
          return StepResult::Error;
      }
    else
      {
        this->current_tuple++;
      }
    if (this->current_tuple < PQntuples(this->result))
      return StepResult::Row;
    return StepResult::Done;
  }

  int64_t get_column_int64(const int col) override
  {
    const char* result = PQgetvalue(this->result, this->current_tuple, col);
    std::istringstream iss;
    iss.str(result);
    int64_t res;
    iss >> res;
    return res;
  }
  std::string get_column_text(const int col) override
  {
    const char* result = PQgetvalue(this->result, this->current_tuple, col);
    return result;
  }
  int get_column_int(const int col) override
  {
    const char* result = PQgetvalue(this->result, this->current_tuple, col);
    std::istringstream iss;
    iss.str(result);
    int res;
    iss >> res;
    return res;
  }

  void bind(std::vector<std::string> params) override
  {

    this->params = std::move(params);
  }

  bool bind_text(const int, const std::string& data) override
  {
    this->params.push_back(data);
    return true;
  }
  bool bind_int64(const int pos, const std::int64_t value) override
  {
    this->params.push_back(std::to_string(value));
    return true;
  }
  bool bind_null(const int pos) override
  {
    this->params.push_back("NULL");
    return true;
  }

 private:

private:
  bool execute()
  {
    std::vector<const char*> params;
    params.reserve(this->params.size());

    for (const auto& param: this->params)
      {
        log_debug("param:", param);
        params.push_back(param.data());
      }

    log_debug("body: ", body);
    this->result = PQexecParams(this->conn, this->body.data(),
                                this->params.size(),
                                nullptr,
                                params.data(),
                                nullptr,
                                nullptr,
                                0);
    const auto status = PQresultStatus(this->result);
    if (status == PGRES_TUPLES_OK)
      {
        log_debug("PGRES_TUPLES_OK");
      }
    else if (status != PGRES_COMMAND_OK)
      {
        log_error("Failed to execute command: ", PQresultErrorMessage(this->result));
        return false;
      }
    return true;
  }

  bool executed{false};
  std::string body;
  PGconn*const conn;
  std::vector<std::string> params;
  PGresult* result{nullptr};
  int current_tuple{0};
};
