#pragma once

#include <biboumi.h>
#ifdef USE_DATABASE

#include <database/schema.hpp>
#include <odb/sqlite/database.hxx>
#include <schema-odb.hpp>

#include <iostream>

#include <memory>

class Database
{
public:
  using Type = odb::sqlite::database;
  using time_point = std::chrono::system_clock::time_point;
  Database() = default;
  ~Database() = default;

  Database(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(const Database&) = delete;
  Database& operator=(Database&&) = delete;

  static void set_verbose(const bool val);

  template <typename T>
  static auto persist(T&& object)
  {
    odb::core::transaction t(Database::db->begin());
    try {
        Database::db->update(std::forward<T>(object));
    } catch (const odb::object_not_persistent&) {
        Database::db->persist(std::forward<T>(object));
    }
    t.commit();
  }

  template <typename T>
  static std::size_t count()
  {
    odb::core::transaction t(Database::db->begin());
    auto results = Database::db->query<T>();
    auto size = std::distance(std::begin(results), std::end(results));
    t.commit();
    return size;
  }

  static db::GlobalOptions get_global_options(const std::string& owner);
  static db::IrcServerOptions get_irc_server_options(const std::string& owner,
                                                     const std::string& server);
  static db::IrcChannelOptions get_irc_channel_options(const std::string& owner,
                                                       const std::string& server,
                                                       const std::string& channel);
  static db::IrcChannelOptions get_irc_channel_options_with_server_default(const std::string& owner,
                                                                           const std::string& server,
                                                                           const std::string& channel);
  static db::IrcChannelOptions get_irc_channel_options_with_server_and_global_default(const std::string& owner,
                                                                                      const std::string& server,
                                                                                      const std::string& channel);
  static std::vector<db::MucLogLine> get_muc_logs(const std::string& owner, const std::string& chan_name, const std::string& server,
                                                  int limit=-1, const std::string& start="", const std::string& end="");
  static std::string store_muc_message(const std::string& owner, const std::string& channel_name, const std::string& server_name,
                                       time_point date, const std::string& body, const std::string& nick);

  static void close();
  static void open(const std::string& filename);


private:
  static std::string gen_uuid();
  static std::unique_ptr<Type> db;
};
#endif /* USE_DATABASE */


