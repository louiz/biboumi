#pragma once

#include <database/update_query.hpp>
#include <database/insert_query.hpp>

#include <database/engine.hpp>

#include <database/row.hpp>

#include <utils/is_one_of.hpp>

template <typename... T, bool Coucou=true>
void save(Row<T...>& row, DatabaseEngine& db, typename std::enable_if<!is_one_of<Id, T...> && Coucou>::type* = nullptr)
{
  insert(row, db);
}

template <typename... T, bool Coucou=true>
void save(Row<T...>& row, DatabaseEngine& db, typename std::enable_if<is_one_of<Id, T...> && Coucou>::type* = nullptr)
{
  const Id& id = std::get<Id>(row.columns);
    if (id.value == Id::unset_value)
      {
        insert(row, db);
        if (db.last_inserted_rowid >= 0)
          std::get<Id>(row.columns).value = static_cast<Id::real_type>(db.last_inserted_rowid);
      }
    else
      update(row, db);
}

