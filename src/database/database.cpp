#include "biboumi.h"
#ifdef USE_DATABASE

#include <database/database.hpp>
#include <logger/logger.hpp>
#include <irc/iid.hpp>
#include <uuid.h>
#include <utils/get_first_non_empty.hpp>
#include <utils/time.hpp>

using namespace std::string_literals;

std::unique_ptr<db::BibouDB> Database::db;

void Database::open(const std::string& filename, const std::string& db_type)
{
  try
    {
      auto new_db = std::make_unique<db::BibouDB>(db_type,
                                             "database="s + filename);
      if (new_db->needsUpgrade())
        new_db->upgrade();
      Database::db.reset(new_db.release());
    } catch (const litesql::DatabaseError& e) {
      log_error("Failed to open database ", filename, ". ", e.what());
      throw;
    }
}

void Database::set_verbose(const bool val)
{
  Database::db->verbose = val;
}

db::GlobalOptions Database::get_global_options(const std::string& owner)
{
  try {
    auto options = litesql::select<db::GlobalOptions>(*Database::db,
                                                      db::GlobalOptions::Owner == owner).one();
    return options;
  } catch (const litesql::NotFound& e) {
    db::GlobalOptions options(*Database::db);
    options.owner = owner;
    return options;
  }
}

db::IrcServerOptions Database::get_irc_server_options(const std::string& owner,
                                                      const std::string& server)
{
  try {
    auto options = litesql::select<db::IrcServerOptions>(*Database::db,
                             db::IrcServerOptions::Owner == owner &&
                             db::IrcServerOptions::Server == server).one();
    return options;
  } catch (const litesql::NotFound& e) {
    db::IrcServerOptions options(*Database::db);
    options.owner = owner;
    options.server = server;
    // options.update();
    return options;
  }
}

db::IrcChannelOptions Database::get_irc_channel_options(const std::string& owner,
                                                        const std::string& server,
                                                        const std::string& channel)
{
  try {
    auto options = litesql::select<db::IrcChannelOptions>(*Database::db,
                                                         db::IrcChannelOptions::Owner == owner &&
                                                         db::IrcChannelOptions::Server == server &&
                                                         db::IrcChannelOptions::Channel == channel).one();
    return options;
  } catch (const litesql::NotFound& e) {
    db::IrcChannelOptions options(*Database::db);
    options.owner = owner;
    options.server = server;
    options.channel = channel;
    return options;
  }
}

db::IrcChannelOptions Database::get_irc_channel_options_with_server_default(const std::string& owner,
                                                                            const std::string& server,
                                                                            const std::string& channel)
{
  auto coptions = Database::get_irc_channel_options(owner, server, channel);
  auto soptions = Database::get_irc_server_options(owner, server);

  coptions.encodingIn = get_first_non_empty(coptions.encodingIn.value(),
                                            soptions.encodingIn.value());
  coptions.encodingOut = get_first_non_empty(coptions.encodingOut.value(),
                                             soptions.encodingOut.value());

  coptions.maxHistoryLength = get_first_non_empty(coptions.maxHistoryLength.value(),
                                                  soptions.maxHistoryLength.value());

  return coptions;
}

db::IrcChannelOptions Database::get_irc_channel_options_with_server_and_global_default(const std::string& owner,
                                                                                       const std::string& server,
                                                                                       const std::string& channel)
{
  auto coptions = Database::get_irc_channel_options(owner, server, channel);
  auto soptions = Database::get_irc_server_options(owner, server);
  auto goptions = Database::get_global_options(owner);

  coptions.encodingIn = get_first_non_empty(coptions.encodingIn.value(),
                                            soptions.encodingIn.value());
  coptions.encodingOut = get_first_non_empty(coptions.encodingOut.value(),
                                             soptions.encodingOut.value());

  coptions.maxHistoryLength = get_first_non_empty(coptions.maxHistoryLength.value(),
                                                  soptions.maxHistoryLength.value(),
                                                  goptions.maxHistoryLength.value());

  return coptions;
}

void Database::store_muc_message(const std::string& owner, const Iid& iid,
                                 Database::time_point date,
                                 const std::string& body,
                                 const std::string& nick)
{
  db::MucLogLine line(*Database::db);

  line.uuid = Database::gen_uuid();
  line.owner = owner;
  line.ircChanName = iid.get_local();
  line.ircServerName = iid.get_server();
  line.date = date.time_since_epoch().count() / 1'000'000'000;
  line.body = body;
  line.nick = nick;

  line.update();
}

std::vector<db::MucLogLine> Database::get_muc_logs(const std::string& owner, const std::string& chan_name, const std::string& server,
                                                   int limit, const std::string& start, const std::string& end)
{
  auto request = litesql::select<db::MucLogLine>(*Database::db,
                                              db::MucLogLine::Owner == owner &&
                                              db::MucLogLine::IrcChanName == chan_name &&
                                              db::MucLogLine::IrcServerName == server);
  request.orderBy(db::MucLogLine::Id, false);

  if (limit >= 0)
    request.limit(limit);
  if (!start.empty())
    {
      const auto start_time = utils::parse_datetime(start);
      if (start_time != -1)
        request.where(db::MucLogLine::Date >= start_time);
    }
  if (!end.empty())
    {
      const auto end_time = utils::parse_datetime(end);
      if (end_time != -1)
        request.where(db::MucLogLine::Date <= end_time);
    }
  const auto& res = request.all();
  return {res.crbegin(), res.crend()};
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
