#include "biboumi.h"
#ifdef USE_DATABASE

#include <database/database.hpp>
#include <config/config.hpp>
#include <utils/xdg.hpp>
#include <logger/logger.hpp>
#include <string>

using namespace std::string_literals;

std::unique_ptr<db::BibouDB> Database::db;

db::BibouDB& Database::get_db()
{
  if (!Database::db)
    {
      const std::string db_filename = Config::get("db_name",
                                                  xdg_data_path("biboumi.sqlite"));
      Database::db = std::make_unique<db::BibouDB>("sqlite3",
                                                   "database="s + db_filename);
    }

  if (Database::db->needsUpgrade())
    Database::db->upgrade();

  return *Database::db.get();
}

void Database::set_verbose(const bool val)
{
  Database::get_db().verbose = val;
}

db::IrcServerOptions Database::get_irc_server_options(const std::string& owner,
                                                      const std::string& server)
{
  try {
    auto options = litesql::select<db::IrcServerOptions>(Database::get_db(),
                             db::IrcServerOptions::Owner == owner &&
                             db::IrcServerOptions::Server == server).one();
    return options;
  } catch (const litesql::NotFound& e) {
    db::IrcServerOptions options(Database::get_db());
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
    auto options = litesql::select<db::IrcChannelOptions>(Database::get_db(),
                                                         db::IrcChannelOptions::Owner == owner &&
                                                         db::IrcChannelOptions::Server == server &&
                                                         db::IrcChannelOptions::Channel == channel).one();
    return options;
  } catch (const litesql::NotFound& e) {
    db::IrcChannelOptions options(Database::get_db());
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
