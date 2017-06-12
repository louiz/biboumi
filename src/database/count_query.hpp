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
      if (sqlite3_step(statement) != SQLITE_DONE)
        {
          log_error("Failed to execute count request.");
          return 0;
        }
      auto res = sqlite3_column_int64(statement, 0);
      log_debug("Returning count: ", res);
      return res;
    }
};
