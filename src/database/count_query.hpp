#pragma once

#include <database/query.hpp>
#include <database/table.hpp>

#include <string>

#include <sqlite3.h>

struct CountQuery: public Query
{
    CountQuery(std::string name):
        Query("SELECT count(*) FROM ")
    {
      this->body += std::move(name);
    }

    std::size_t execute(sqlite3* db)
    {
      auto statement = this->prepare(db);
      std::size_t res = 0;
      if (sqlite3_step(statement.get()) == SQLITE_ROW)
        res = sqlite3_column_int64(statement.get(), 0);
      else
        {
          log_error("Count request didn’t return a result");
          return 0;
        }
      if (sqlite3_step(statement.get()) != SQLITE_DONE)
        log_warning("Count request returned more than one result.");

      log_debug("Returning count: ", res);
      return res;
    }
};
