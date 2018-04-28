#pragma once

#include <biboumi.h>
#ifdef USE_DATABASE

#include <database/table.hpp>
#include <database/column.hpp>
#include <database/count_query.hpp>

#include <database/engine.hpp>

#include <utils/optional_bool.hpp>

#include <chrono>
#include <string>

#include <memory>
#include <map>


class Database
{
 public:
  using time_point = std::chrono::system_clock::time_point;
  struct RecordNotFound: public std::exception {};
  enum class Paging { first, last };

  struct Uuid: Column<std::string> { static constexpr auto name = "uuid_"; };

  struct Owner: UnclearableColumn<std::string> { static constexpr auto name = "owner_"; };

  struct IrcChanName: UnclearableColumn<std::string> { static constexpr auto name = "ircchanname_"; };

  struct Channel: UnclearableColumn<std::string> { static constexpr auto name = "channel_"; };

  struct IrcServerName: UnclearableColumn<std::string> { static constexpr auto name = "ircservername_"; };

  struct Server: UnclearableColumn<std::string> { static constexpr auto name = "server_"; };

  struct Date: Column<time_point::rep> { static constexpr auto name = "date_"; };

  struct Body: Column<std::string> { static constexpr auto name = "body_"; };

  struct Nick: Column<std::string> { static constexpr auto name = "nick_"; };

  struct Pass: Column<std::string> { static constexpr auto name = "pass_"; };

  struct Ports: Column<std::string> { static constexpr auto name = "ports_";
    Ports(): Column<std::string>("6667") {} };

  struct TlsPorts: Column<std::string> { static constexpr auto name = "tlsports_";
    TlsPorts(): Column<std::string>("6697;6670") {} };

  struct Username: Column<std::string> { static constexpr auto name = "username_"; };

  struct Realname: Column<std::string> { static constexpr auto name = "realname_"; };

  struct AfterConnectionCommand: Column<std::string> { static constexpr auto name = "afterconnectioncommand_"; };

  struct TrustedFingerprint: Column<std::string> { static constexpr auto name = "trustedfingerprint_"; };

  struct EncodingOut: Column<std::string> { static constexpr auto name = "encodingout_"; };

  struct EncodingIn: Column<std::string> { static constexpr auto name = "encodingin_"; };

  struct MaxHistoryLength: Column<int> { static constexpr auto name = "maxhistorylength_";
    MaxHistoryLength(): Column<int>(20) {} };

  struct RecordHistory: Column<bool> { static constexpr auto name = "recordhistory_";
    RecordHistory(): Column<bool>(true) {}};

  struct RecordHistoryOptional: Column<OptionalBool> { static constexpr auto name = "recordhistory_"; };

  struct VerifyCert: Column<bool> { static constexpr auto name = "verifycert_";
    VerifyCert(): Column<bool>(true) {} };

  struct Persistent: Column<bool> { static constexpr auto name = "persistent_";
    Persistent(): Column<bool>(false) {} };

  struct GlobalPersistent: Column<bool> { static constexpr auto name = "persistent_";
    GlobalPersistent(); };

  struct LocalJid: Column<std::string> { static constexpr auto name = "local"; };

  struct RemoteJid: Column<std::string> { static constexpr auto name = "remote"; };

  struct Address: Column<std::string> { static constexpr auto name = "address_"; };

  using MucLogLineTable = Table<Id, Uuid, Owner, IrcChanName, IrcServerName, Date, Body, Nick>;
  using MucLogLine = MucLogLineTable::RowType;

  using GlobalOptionsTable = Table<Id, Owner, MaxHistoryLength, RecordHistory, GlobalPersistent>;
  using GlobalOptions = GlobalOptionsTable::RowType;

  using IrcServerOptionsTable = Table<Id, Owner, Server, Pass, TlsPorts, Ports, Username, Realname, VerifyCert, TrustedFingerprint, EncodingOut, EncodingIn, MaxHistoryLength, Address, Nick>;
  using IrcServerOptions = IrcServerOptionsTable::RowType;

  using IrcChannelOptionsTable = Table<Id, Owner, Server, Channel, EncodingOut, EncodingIn, MaxHistoryLength, Persistent, RecordHistoryOptional>;
  using IrcChannelOptions = IrcChannelOptionsTable::RowType;

  using RosterTable = Table<LocalJid, RemoteJid>;
  using RosterItem = RosterTable::RowType;

  using AfterConnectionCommandsTable = Table<Id, ForeignKey, AfterConnectionCommand>;
  using AfterConnectionCommands = std::vector<AfterConnectionCommandsTable::RowType>;

  Database() = default;
  ~Database() = default;

  Database(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(const Database&) = delete;
  Database& operator=(Database&&) = delete;

  static GlobalOptions get_global_options(const std::string& owner);
  static IrcServerOptions get_irc_server_options(const std::string& owner,
                                                     const std::string& server);
  static IrcChannelOptions get_irc_channel_options(const std::string& owner,
                                                   const std::string& server,
                                                   const std::string& channel);
  static IrcChannelOptions get_irc_channel_options_with_server_default(const std::string& owner,
                                                                       const std::string& server,
                                                                       const std::string& channel);
  static IrcChannelOptions get_irc_channel_options_with_server_and_global_default(const std::string& owner,
                                                                                  const std::string& server,
                                                                                  const std::string& channel);
  static AfterConnectionCommands get_after_connection_commands(const IrcServerOptions& server_options);
  static void set_after_connection_commands(const IrcServerOptions& server_options, AfterConnectionCommands& commands);

  /**
   * Get all the lines between (optional) start and end dates, with a (optional) limit.
   * If after_id is set, only the records after it will be returned.
   */
  static std::vector<MucLogLine> get_muc_logs(const std::string& owner, const std::string& chan_name, const std::string& server,
                                              int limit=-1, const std::string& start="", const std::string& end="",
                                              const Id::real_type reference_record_id=Id::unset_value, Paging=Paging::first);

  /**
   * Get just one single record matching the given uuid, between (optional) end and start.
   * If it does not exist (or is not between end and start), throw a RecordNotFound exception.
   */
  static MucLogLine get_muc_log(const std::string& owner, const std::string& chan_name, const std::string& server, const std::string& uuid, const std::string& start="", const std::string& end="");
  static std::string store_muc_message(const std::string& owner, const std::string& chan_name, const std::string& server_name,
                                       time_point date, const std::string& body, const std::string& nick);

  static void add_roster_item(const std::string& local, const std::string& remote);
  static bool has_roster_item(const std::string& local, const std::string& remote);
  static void delete_roster_item(const std::string& local, const std::string& remote);
  static std::vector<Database::RosterItem> get_contact_list(const std::string& local);
  static std::vector<Database::RosterItem> get_full_roster();

  static void close();
  static void open(const std::string& filename);

  template <typename TableType>
  static int64_t count(const TableType& table)
  {
    CountQuery query{table.get_name()};
    return query.execute(*Database::db);
  }

  static MucLogLineTable muc_log_lines;
  static GlobalOptionsTable global_options;
  static IrcServerOptionsTable irc_server_options;
  static IrcChannelOptionsTable irc_channel_options;
  static RosterTable roster;
  static AfterConnectionCommandsTable after_connection_commands;

  static std::unique_ptr<DatabaseEngine> db;

  /**
   * Some caches, to avoid doing very frequent query requests for a few options.
   */
  using CacheKey = std::tuple<std::string, std::string, std::string>;

  static EncodingIn::real_type get_encoding_in(const std::string& owner,
                                        const std::string& server,
                                        const std::string& channel)
  {
    CacheKey channel_key{owner, server, channel};
    auto it = Database::encoding_in_cache.find(channel_key);
    if (it == Database::encoding_in_cache.end())
      {
        auto options = Database::get_irc_channel_options_with_server_default(owner, server, channel);
        EncodingIn::real_type result = options.col<Database::EncodingIn>();
        if (result.empty())
          result = "ISO-8859-1";
        it = Database::encoding_in_cache.insert(std::make_pair(channel_key, result)).first;
      }
    return it->second;
  }
  static void invalidate_encoding_in_cache(const std::string& owner,
                                           const std::string& server,
                                           const std::string& channel)
  {
    CacheKey channel_key{owner, server, channel};
    Database::encoding_in_cache.erase(channel_key);
  }
  static void invalidate_encoding_in_cache()
  {
    Database::encoding_in_cache.clear();
  }

  static auto raw_exec(const std::string& query)
  {
    return Database::db->raw_exec(query);
  }

 private:
  static std::string gen_uuid();
  static std::map<CacheKey, EncodingIn::real_type> encoding_in_cache;
};

class Transaction
{
public:
  Transaction();
  ~Transaction();
  bool success{false};
};

#endif /* USE_DATABASE */
