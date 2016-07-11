#include "biboumi.h"
#ifdef USE_DATABASE

#include <database/database.hpp>
#include <logger/logger.hpp>
#include <string>

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
  if (coptions.encodingIn.value().empty())
    coptions.encodingIn = soptions.encodingIn;
  if (coptions.encodingOut.value().empty())
    coptions.encodingOut = soptions.encodingOut;

  return coptions;
}

void Database::close()
{
  Database::db.reset(nullptr);
}

#endif
