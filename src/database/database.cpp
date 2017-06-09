#include "biboumi.h"
#ifdef USE_DATABASE

#include <database/schema.hpp>
#include <database/old_schema.hpp>

#include <old_schema-odb.hpp>
#include <schema-odb.hpp>

#include <irc/iid.hpp>

#include <database/database.hpp>

#include <odb/database.hxx>
#include <odb/schema-catalog.hxx>
#include <odb/transaction.hxx>

#include <logger/logger.hpp>
#include <irc/iid.hpp>
#include <uuid/uuid.h>
#include <utils/get_first_non_empty.hpp>
#include <utils/time.hpp>

using namespace std::string_literals;

std::unique_ptr<Database::Type> Database::db;

namespace
{
template <typename From, typename To>
void migrate(Database::Type& db)
{
  try {
      odb::core::transaction t(db.begin());
      auto res = db.template query<From>();

      for (const auto& val: res)
        {
          To entry(val);
          db.persist(entry);
        }
      t.commit();
    } catch (...) {
      // The table doesn’t exist, no data to migrate
      return;
    }

}
}

void Database::open(const std::string& filename)
{
  Database::db = std::make_unique<Database::Type>(filename, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE);

  if (db->schema_version() == 0)
    { // New schema does not exist at all. Create it and migrate old data if they exist
      log_debug("Creating new database schema…");
      {
        odb::core::transaction t(db->begin());
        odb::schema_catalog::create_schema(*db);
        t.commit();
      }
      log_debug("Migrate data");
      {
        migrate<old_database::MucLogLine, db::MucLogLine>(*db);
        migrate<old_database::GlobalOptions, db::GlobalOptions>(*db);
        migrate<old_database::IrcServerOptions, db::IrcServerOptions>(*db);
        migrate<old_database::IrcChannelOptions, db::IrcChannelOptions>(*db);
      }
      log_debug("Done.");
    }
}

void Database::set_verbose(const bool)
{
}

db::GlobalOptions Database::get_global_options(const std::string& owner)
{
  using Query = odb::query<db::GlobalOptions>;

  Query query(Query::owner == owner);
  db::GlobalOptions options;
  options.owner = owner;

  odb::core::transaction t(db->begin());
  Database::db->query_one(query, options);
  t.commit();

  return options;
}

db::IrcServerOptions Database::get_irc_server_options(const std::string& owner,
                                                      const std::string& server)
{
  using Query = odb::query<db::IrcServerOptions>;

  Query query(Query::owner == owner && Query::server == server);
  db::IrcServerOptions options;
  options.owner = owner;
  options.server = server;
  odb::core::transaction t(db->begin());
  Database::db->query_one(query, options);
  t.commit();
  return options;
}

db::IrcChannelOptions Database::get_irc_channel_options(const std::string& owner,
                                                        const std::string& server,
                                                        const std::string& channel)
{
  using Query = odb::query<db::IrcChannelOptions>;

  Query query(Query::owner == owner && Query::server == server && Query::channel == channel);
  db::IrcChannelOptions options;
  options.owner = owner;
  options.server = server;
  options.channel = channel;
  odb::core::transaction t(db->begin());
  Database::db->query_one(query, options);
t.commit();
  return options;
}

db::IrcChannelOptions Database::get_irc_channel_options_with_server_default(const std::string& owner,
                                                                            const std::string& server,
                                                                            const std::string& channel)
{
  auto coptions = Database::get_irc_channel_options(owner, server, channel);
  auto soptions = Database::get_irc_server_options(owner, server);

  coptions.encoding_in = get_first_non_empty(coptions.encoding_in,
                                            soptions.encoding_in);
  coptions.encoding_out = get_first_non_empty(coptions.encoding_out,
                                             soptions.encoding_out);

  coptions.max_history_length = get_first_non_empty(coptions.max_history_length,
                                                  soptions.max_history_length);

  return coptions;
}

db::IrcChannelOptions Database::get_irc_channel_options_with_server_and_global_default(const std::string& owner,
                                                                                       const std::string& server,
                                                                                       const std::string& channel)
{
  auto coptions = Database::get_irc_channel_options(owner, server, channel);
  auto soptions = Database::get_irc_server_options(owner, server);
  auto goptions = Database::get_global_options(owner);

  coptions.encoding_in = get_first_non_empty(coptions.encoding_in,
                                            soptions.encoding_in);
  coptions.encoding_out = get_first_non_empty(coptions.encoding_out,
                                             soptions.encoding_out);

  coptions.max_history_length = get_first_non_empty(coptions.max_history_length,
                                                  soptions.max_history_length,
                                                  goptions.max_history_length);

  return coptions;
}

std::string Database::store_muc_message(const std::string& owner, const std::string& channel_name,
                                        const std::string& server_name,
                                        Database::time_point date,
                                        const std::string& body,
                                        const std::string& nick)
{
  db::MucLogLine line;

  auto uuid = Database::gen_uuid();

  line.uuid = uuid;
  line.owner = owner;
  line.channel_name = channel_name;
  line.server_name = server_name;
  line.date = std::chrono::duration_cast<std::chrono::seconds>(date.time_since_epoch()).count();
  line.body = body;
  line.nick = nick;
  odb::core::transaction t(db->begin());

  Database::db->persist(line);
  t.commit();
  return uuid;
}

std::vector<db::MucLogLine> Database::get_muc_logs(const std::string& owner, const std::string& chan_name, const std::string& server,
                                                   int limit, const std::string& start, const std::string& end)
{

  using Query = odb::query<db::MucLogLine>;


  Query query{Query::owner == owner &&
              Query::channel_name == chan_name &&
              Query::server_name == server};

  if (!start.empty())
    {
      const auto start_time = utils::parse_datetime(start);
      if (start_time != -1)
        query = query && (Query::date >= start_time);
    }
  if (!end.empty())
    {
      const auto end_time = utils::parse_datetime(end);
      if (end_time != -1)
        query = query && (Query::date <= end_time);
    }

  query += "ORDER BY" + Query::id + "DESC";

  if (limit >= 0)
    query += "LIMIT" + Query::_val(limit);

  odb::core::transaction t(db->begin());
  odb::result<db::MucLogLine> res = Database::db->query<db::MucLogLine>(query);

  std::vector<db::MucLogLine> results{res.begin(), res.end()};
  t.commit();

  return {results.crbegin(), results.crend()};
}

void Database::close()
{
  Database::db.reset(nullptr);
}

std::string Database::gen_uuid()
{
  char uuid_str[37];
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_str);
  return uuid_str;
}


#endif
