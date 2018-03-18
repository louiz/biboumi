#include "biboumi.h"
#ifdef USE_DATABASE

#include <database/database.hpp>
#include <uuid/uuid.h>
#include <utils/get_first_non_empty.hpp>
#include <utils/time.hpp>

#include <config/config.hpp>
#include <database/sqlite3_engine.hpp>
#include <database/postgresql_engine.hpp>

#include <database/engine.hpp>
#include <database/index.hpp>

#include <memory>

std::unique_ptr<DatabaseEngine> Database::db;
Database::MucLogLineTable Database::muc_log_lines("muclogline_");
Database::GlobalOptionsTable Database::global_options("globaloptions_");
Database::IrcServerOptionsTable Database::irc_server_options("ircserveroptions_");
Database::IrcChannelOptionsTable Database::irc_channel_options("ircchanneloptions_");
Database::RosterTable Database::roster("roster");
Database::AfterConnectionCommandsTable Database::after_connection_commands("after_connection_commands_");
std::map<Database::CacheKey, Database::EncodingIn::real_type> Database::encoding_in_cache{};

Database::GlobalPersistent::GlobalPersistent():
    Column<bool>{Config::get_bool("persistent_by_default", false)}
{}

void Database::open(const std::string& filename)
{
  // Try to open the specified database.
  // Close and replace the previous database pointer if it succeeded. If it did
  // not, just leave things untouched
  std::unique_ptr<DatabaseEngine> new_db;
  static const auto psql_prefix = "postgresql://"s;
  static const auto psql_prefix2 = "postgres://"s;
  if ((filename.substr(0, psql_prefix.size()) == psql_prefix) ||
      (filename.substr(0, psql_prefix2.size()) == psql_prefix2))
    new_db = PostgresqlEngine::open(filename);
  else
    new_db = Sqlite3Engine::open(filename);
  if (!new_db)
    return;
  Database::db = std::move(new_db);
  Database::muc_log_lines.create(*Database::db);
  Database::muc_log_lines.upgrade(*Database::db);
  Database::global_options.create(*Database::db);
  Database::global_options.upgrade(*Database::db);
  Database::irc_server_options.create(*Database::db);
  Database::irc_server_options.upgrade(*Database::db);
  Database::irc_channel_options.create(*Database::db);
  Database::irc_channel_options.upgrade(*Database::db);
  Database::roster.create(*Database::db);
  Database::roster.upgrade(*Database::db);
  Database::after_connection_commands.create(*Database::db);
  Database::after_connection_commands.upgrade(*Database::db);
  create_index<Database::Owner, Database::IrcChanName, Database::IrcServerName>(*Database::db, "archive_index", Database::muc_log_lines.get_name());
}


Database::GlobalOptions Database::get_global_options(const std::string& owner)
{
  auto request = Database::global_options.select();
  request.where() << Owner{} << "=" << owner;

  Database::GlobalOptions options{Database::global_options.get_name()};
  auto result = request.execute(*Database::db);
  if (result.size() == 1)
    options = result.front();
  else
    options.col<Owner>() = owner;
  return options;
}

Database::IrcServerOptions Database::get_irc_server_options(const std::string& owner, const std::string& server)
{
  auto request = Database::irc_server_options.select();
  request.where() << Owner{} << "=" << owner << " and " << Server{} << "=" << server;

  Database::IrcServerOptions options{Database::irc_server_options.get_name()};
  auto result = request.execute(*Database::db);
  if (result.size() == 1)
    options = result.front();
  else
    {
      options.col<Owner>() = owner;
      options.col<Server>() = server;
    }
  return options;
}

Database::AfterConnectionCommands Database::get_after_connection_commands(const IrcServerOptions& server_options)
{
  const auto id = server_options.col<Id>();
  if (id == Id::unset_value)
    return {};
  auto request = Database::after_connection_commands.select();
  request.where() << ForeignKey{} << "=" << id;
  return request.execute(*Database::db);
}

void Database::set_after_connection_commands(const Database::IrcServerOptions& server_options, Database::AfterConnectionCommands& commands)
{
  const auto id = server_options.col<Id>();
  if (id == Id::unset_value)
    return ;

  Transaction transaction;
  auto query = Database::after_connection_commands.del();
  query.where() << ForeignKey{} << "=" << id;
  query.execute(*Database::db);

  for (auto& command: commands)
    {
      command.col<ForeignKey>() = server_options.col<Id>();
      command.save(Database::db);
    }
}

Database::IrcChannelOptions Database::get_irc_channel_options(const std::string& owner, const std::string& server, const std::string& channel)
{
  auto request = Database::irc_channel_options.select();
  request.where() << Owner{} << "=" << owner <<\
          " and " << Server{} << "=" << server <<\
          " and " << Channel{} << "=" << channel;
  Database::IrcChannelOptions options{Database::irc_channel_options.get_name()};
  auto result = request.execute(*Database::db);
  if (result.size() == 1)
    options = result.front();
  else
    {
      options.col<Owner>() = owner;
      options.col<Server>() = server;
      options.col<Channel>() = channel;
    }
  return options;
}

Database::IrcChannelOptions Database::get_irc_channel_options_with_server_default(const std::string& owner, const std::string& server,
                                                                                  const std::string& channel)
{
  auto coptions = Database::get_irc_channel_options(owner, server, channel);
  auto soptions = Database::get_irc_server_options(owner, server);

  coptions.col<EncodingIn>() = get_first_non_empty(coptions.col<EncodingIn>(),
                                                   soptions.col<EncodingIn>());
  coptions.col<EncodingOut>() = get_first_non_empty(coptions.col<EncodingOut>(),
                                                    soptions.col<EncodingOut>());

  coptions.col<MaxHistoryLength>() = get_first_non_empty(coptions.col<MaxHistoryLength>(),
                                                         soptions.col<MaxHistoryLength>());

  return coptions;
}

Database::IrcChannelOptions Database::get_irc_channel_options_with_server_and_global_default(const std::string& owner, const std::string& server, const std::string& channel)
{
  auto coptions = Database::get_irc_channel_options(owner, server, channel);
  auto soptions = Database::get_irc_server_options(owner, server);
  auto goptions = Database::get_global_options(owner);

  coptions.col<EncodingIn>() = get_first_non_empty(coptions.col<EncodingIn>(),
                                                   soptions.col<EncodingIn>());

  coptions.col<EncodingOut>() = get_first_non_empty(coptions.col<EncodingOut>(),
                                                    soptions.col<EncodingOut>());

  coptions.col<MaxHistoryLength>() = get_first_non_empty(coptions.col<MaxHistoryLength>(),
                                                         soptions.col<MaxHistoryLength>(),
                                                         goptions.col<MaxHistoryLength>());

  return coptions;
}

std::string Database::store_muc_message(const std::string& owner, const std::string& chan_name,
                                        const std::string& server_name, Database::time_point date,
                                        const std::string& body, const std::string& nick)
{
  auto line = Database::muc_log_lines.row();

  auto uuid = Database::gen_uuid();

  line.col<Uuid>() = uuid;
  line.col<Owner>() = owner;
  line.col<IrcChanName>() = chan_name;
  line.col<IrcServerName>() = server_name;
  line.col<Date>() = std::chrono::duration_cast<std::chrono::seconds>(date.time_since_epoch()).count();
  line.col<Body>() = body;
  line.col<Nick>() = nick;

  line.save(Database::db);

  return uuid;
}

std::vector<Database::MucLogLine> Database::get_muc_logs(const std::string& owner, const std::string& chan_name, const std::string& server,
                                                   int limit, const std::string& start, const std::string& end, const Id::real_type reference_record_id, Database::Paging paging)
{
  if (limit == 0)
    return {};

  auto request = Database::muc_log_lines.select();
  request.where() << Database::Owner{} << "=" << owner << \
          " and " << Database::IrcChanName{} << "=" << chan_name << \
          " and " << Database::IrcServerName{} << "=" << server;

  if (!start.empty())
    {
      const auto start_time = utils::parse_datetime(start);
      if (start_time != -1)
        request << " and " << Database::Date{} << ">=" << start_time;
    }
  if (!end.empty())
    {
      const auto end_time = utils::parse_datetime(end);
      if (end_time != -1)
        request << " and " << Database::Date{} << "<=" << end_time;
    }
  if (reference_record_id != Id::unset_value)
    {
      request << " and " << Id{};
      if (paging == Database::Paging::first)
        request << ">";
      else
        request << "<";
      request << reference_record_id;
    }

  if (paging == Database::Paging::first)
    request.order_by() << Database::Date{} << " ASC, " << Id{} << " ASC ";
  else
    request.order_by() << Database::Date{} << " DESC, " << Id{} << " DESC ";

  if (limit >= 0)
    request.limit() << limit;

  auto result = request.execute(*Database::db);

  if (paging == Database::Paging::first)
    return result;
  else
    return {result.crbegin(), result.crend()};
}

Database::MucLogLine Database::get_muc_log(const std::string& owner, const std::string& chan_name, const std::string& server,
                                           const std::string& uuid, const std::string& start, const std::string& end)
{
  auto request = Database::muc_log_lines.select();
  request.where() << Database::Owner{} << "=" << owner << \
          " and " << Database::IrcChanName{} << "=" << chan_name << \
          " and " << Database::IrcServerName{} << "=" << server << \
          " and " << Database::Uuid{} << "=" << uuid;

  if (!start.empty())
    {
      const auto start_time = utils::parse_datetime(start);
      if (start_time != -1)
        request << " and " << Database::Date{} << ">=" << start_time;
    }
  if (!end.empty())
    {
      const auto end_time = utils::parse_datetime(end);
      if (end_time != -1)
        request << " and " << Database::Date{} << "<=" << end_time;
    }

  auto result = request.execute(*Database::db);

  if (result.empty())
    throw Database::RecordNotFound{};
  return result.front();
}

void Database::add_roster_item(const std::string& local, const std::string& remote)
{
  auto roster_item = Database::roster.row();

  roster_item.col<Database::LocalJid>() = local;
  roster_item.col<Database::RemoteJid>() = remote;

  roster_item.save(Database::db);
}

void Database::delete_roster_item(const std::string& local, const std::string& remote)
{
  Query query("DELETE FROM "s + Database::roster.get_name());
  query << " WHERE " << Database::RemoteJid{} << "=" << remote << \
           " AND " << Database::LocalJid{} << "=" << local;

//  query.execute(*Database::db);
}

bool Database::has_roster_item(const std::string& local, const std::string& remote)
{
  auto query = Database::roster.select();
  query.where() << Database::LocalJid{} << "=" << local << \
        " and " << Database::RemoteJid{} << "=" << remote;

  auto res = query.execute(*Database::db);

  return !res.empty();
}

std::vector<Database::RosterItem> Database::get_contact_list(const std::string& local)
{
  auto query = Database::roster.select();
  query.where() << Database::LocalJid{} << "=" << local;

  return query.execute(*Database::db);
}

std::vector<Database::RosterItem> Database::get_full_roster()
{
  auto query = Database::roster.select();

  return query.execute(*Database::db);
}

void Database::close()
{
  Database::db = nullptr;
}

std::string Database::gen_uuid()
{
  char uuid_str[37];
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_str);
  return uuid_str;
}

Transaction::Transaction()
{
  const auto result = Database::raw_exec("BEGIN");
  if (std::get<bool>(result) == false)
    log_error("Failed to create SQL transaction: ", std::get<std::string>(result));
  else
    this->success = true;

}

Transaction::~Transaction()
{
  if (this->success)
    {
      const auto result = Database::raw_exec("END");
      if (std::get<bool>(result) == false)
        log_error("Failed to end SQL transaction: ", std::get<std::string>(result));
    }
}
#endif
