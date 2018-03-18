#pragma once

#include <database/query.hpp>
#include <database/engine.hpp>

class DeleteQuery: public Query
{
public:
  DeleteQuery(const std::string& name):
      Query("DELETE")
  {
    this->body += " from " + name;
  }

  DeleteQuery& where()
  {
    this->body += " WHERE ";
    return *this;
  };

  void execute(DatabaseEngine& db)
  {
    auto statement = db.prepare(this->body);
    if (!statement)
      return;
#ifdef DEBUG_SQL_QUERIES
    const auto timer = this->log_and_time();
#endif
    statement->bind(std::move(this->params));
    if (statement->step() != StepResult::Done)
      log_error("Failed to execute DELETE command");
  }
};
