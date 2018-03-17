#pragma once

#include <database/insert_query.hpp>
#include <database/update_query.hpp>
#include <logger/logger.hpp>

#include <utils/is_one_of.hpp>

#include <type_traits>

template <typename... T>
struct Row
{
  Row(std::string name):
      table_name(std::move(name))
  {}

  template <typename Type>
  typename Type::real_type& col()
  {
    auto&& col = std::get<Type>(this->columns);
    return col.value;
  }

  template <typename Type>
  const auto& col() const
  {
    auto&& col = std::get<Type>(this->columns);
    return col.value;
  }

  void save(std::unique_ptr<DatabaseEngine>& db)
  {
    if constexpr(is_one_of<Id, T...>)
      {
        const Id& id = std::get<Id>(this->columns);
        if (id.value == Id::unset_value)
          {
            this->insert(*db);
            if (db->last_inserted_rowid >= 0)
              std::get<Id>(this->columns).value = static_cast<Id::real_type>(db->last_inserted_rowid);
          }
        else
          this->update(*db);
      }
    else
      this->insert(*db);
  }

 private:
  void insert(DatabaseEngine& db)
  {
    if constexpr(is_one_of<Id, T...>)
      {
        InsertQuery query(this->table_name, this->columns);
        // Ugly workaround for non portable stuff
        query.body += db.get_returning_id_sql_string(Id::name);
        query.execute(db, this->columns);
      }
    else
      {
        InsertQuery query(this->table_name, this->columns);
        query.execute(db, this->columns);
      }
  }

  void update(DatabaseEngine& db)
  {
    UpdateQuery query(this->table_name, this->columns);

    query.execute(db, this->columns);
  }

public:
  std::tuple<T...> columns;
  std::string table_name;

};
