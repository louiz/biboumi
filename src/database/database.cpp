#include "biboumi.h"

#include <database/database.hpp>
#include <utils/time.hpp>
#include <uuid.h>
#include <utils/get_first_non_empty.hpp>

sqlite3* Database::db;
Database::MucLogLineTable Database::muc_log_lines(Database::db, "MucLogLine_");
Database::GlobalOptionsTable Database::global_options(Database::db, "GlobalOptions_");
Database::IrcServerOptionsTable Database::irc_server_options(Database::db, "IrcServerOptions_");
Database::IrcChannelOptionsTable Database::irc_channel_options(Database::db, "IrcChannelOptions_");

std::string Database::gen_uuid()
{
  char uuid_str[37];
  uuid_t uuid;
  uuid_generate(uuid);
  uuid_unparse(uuid, uuid_str);
  return uuid_str;
}

void Database::open(const std::string& filename)
{
  auto res = sqlite3_open_v2(filename.data(), &Database::db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
  log_debug("open: ", res);
  Database::muc_log_lines.create();
  Database::global_options.create();
  Database::irc_server_options.create();
  Database::irc_channel_options.create();
}

void Database::close()
{
}

Database::GlobalOptions Database::get_global_options(const std::string& owner)
{
  auto request = Database::global_options.select().where() << Owner{} << "=" << owner;
  auto result = request.execute(Database::db);
  if (result.size() == 1)
    return result.front();
  return {Database::global_options.get_name()};
}

Database::IrcServerOptions Database::get_irc_server_options(const std::string& owner, const std::string& server)
{
  auto request = Database::irc_server_options.select().where() << Owner{} << "=" << owner << " and " << Server{} << "=" << server;
  auto result = request.execute(Database::db);
  if (result.size() == 1)
    return result.front();
  return {Database::irc_server_options.get_name()};
}

Database::IrcChannelOptions Database::get_irc_channel_options(const std::string& owner, const std::string& server, const std::string& channel)
{
  auto request = Database::irc_channel_options.select().where() << Owner{} << "=" << owner <<\
                                                        " and " << Server{} << "=" << server <<\
                                                        " and " << Channel{} << "=" << channel;
  auto result = request.execute(Database::db);
  if (result.size() == 1)
    return result.front();
  return {Database::irc_channel_options.get_name()};
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

std::vector<Database::MucLogLine> Database::get_muc_logs(const std::string& owner, const std::string& chan_name, const std::string& server,
                                                   int limit, const std::string& start, const std::string& end)
{
  auto request = Database::muc_log_lines.select().where() << Database::Owner{} << "=" << owner << \
                                            " and " << Database::IrcChanName{} << "=" << chan_name << \
                                            " and " << Database::IrcServerName{} << "=" << server;

  if (!start.empty())
    {
      const auto start_time = utils::parse_datetime(start);
      if (start_time != -1)
        request << Database::Date{} << ">=" << start_time;
    }
  if (!end.empty())
    {
      const auto end_time = utils::parse_datetime(end);
      if (end_time != -1)
        request << Database::Date{} << "<=" << end_time;
    }

  request.order_by() << Id{};
  auto result = request.execute(Database::db);

  if (limit >= 0)
    request.limit() << limit;

  return result;
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
