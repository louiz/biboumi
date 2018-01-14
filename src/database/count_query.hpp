#pragma once

#include <database/query.hpp>
#include <database/table.hpp>
#include <database/statement.hpp>

#include <string>

struct CountQuery: public Query
{
    CountQuery(std::string name):
        Query("SELECT count(*) FROM ")
    {
      this->body += std::move(name);
    }

    int64_t execute(DatabaseEngine& db)
    {
#ifdef DEBUG_SQL_QUERIES
      const auto timer = this->log_and_time();
#endif
      auto statement = db.prepare(this->body);
      int64_t res = 0;
      if (statement->step() != StepResult::Error)
        res = statement->get_column_int64(0);
      else
        {
          log_error("Count request didn’t return a result");
          return 0;
        }
      return res;
    }
};
