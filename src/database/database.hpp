#pragma once

#include <biboumi.h>
#ifdef USE_DATABASE

#include <database/table.hpp>
#include <database/column.hpp>
#include <database/count_query.hpp>

#include <utils/optional_bool.hpp>

#include <chrono>
#include <string>

#include <memory>


class Database
{
 public:
  using time_point = std::chrono::system_clock::time_point;

  struct Uuid: Column<std::string> { static constexpr auto name = "uuid_"; };

  struct Owner: Column<std::string> { static constexpr auto name = "owner_"; };

  struct IrcChanName: Column<std::string> { static constexpr auto name = "ircChanName_"; };

  struct Channel: Column<std::string> { static constexpr auto name = "channel_"; };

  struct IrcServerName: Column<std::string> { static constexpr auto name = "ircServerName_"; };

  struct Server: Column<std::string> { static constexpr auto name = "server_"; };

  struct Date: Column<time_point::rep> { static constexpr auto name = "date_"; };

  struct Body: Column<std::string> { static constexpr auto name = "body_"; };

  struct Nick: Column<std::string> { static constexpr auto name = "nick_"; };

  struct Pass: Column<std::string> { static constexpr auto name = "pass_"; };

  struct Ports: Column<std::string> { static constexpr auto name = "ports_";
    Ports(): Column<std::string>("6667") {} };

  struct TlsPorts: Column<std::string> { static constexpr auto name = "tlsPorts_";
    TlsPorts(): Column<std::string>("6697;6670") {} };

  struct Username: Column<std::string> { static constexpr auto name = "username_"; };

  struct Realname: Column<std::string> { static constexpr auto name = "realname_"; };

  struct AfterConnectionCommand: Column<std::string> { static constexpr auto name = "afterConnectionCommand_"; };

  struct TrustedFingerprint: Column<std::string> { static constexpr auto name = "trustedFingerprint_"; };

  struct EncodingOut: Column<std::string> { static constexpr auto name = "encodingOut_"; };

  struct EncodingIn: Column<std::string> { static constexpr auto name = "encodingIn_"; };

  struct MaxHistoryLength: Column<int> { static constexpr auto name = "maxHistoryLength_";
    MaxHistoryLength(): Column<int>(20) {} };

  struct RecordHistory: Column<bool> { static constexpr auto name = "recordHistory_";
    RecordHistory(): Column<bool>(true) {}};

  struct RecordHistoryOptional: Column<OptionalBool> { static constexpr auto name = "recordHistory_"; };

  struct VerifyCert: Column<bool> { static constexpr auto name = "verifyCert_";
    VerifyCert(): Column<bool>(true) {} };

  struct Persistent: Column<bool> { static constexpr auto name = "persistent_";
    Persistent(): Column<bool>(false) {} };

  struct LocalJid: Column<std::string> { static constexpr auto name = "local";
   static constexpr auto options = ""; };

  struct RemoteJid: Column<std::string> { static constexpr auto name = "remote";
   static constexpr auto options = ""; };


  using MucLogLineTable = Table<Id, Uuid, Owner, IrcChanName, IrcServerName, Date, Body, Nick>;
  using MucLogLine = MucLogLineTable::RowType;

  using GlobalOptionsTable = Table<Id, Owner, MaxHistoryLength, RecordHistory, Persistent>;
  using GlobalOptions = GlobalOptionsTable::RowType;

  using IrcServerOptionsTable = Table<Id, Owner, Server, Pass, AfterConnectionCommand, TlsPorts, Ports, Username, Realname, VerifyCert, TrustedFingerprint, EncodingOut, EncodingIn, MaxHistoryLength>;
  using IrcServerOptions = IrcServerOptionsTable::RowType;

  using IrcChannelOptionsTable = Table<Id, Owner, Server, Channel, EncodingOut, EncodingIn, MaxHistoryLength, Persistent, RecordHistoryOptional>;
  using IrcChannelOptions = IrcChannelOptionsTable::RowType;

  using RosterTable = Table<LocalJid, RemoteJid>;
  using RosterItem = RosterTable::RowType;

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
  static std::vector<MucLogLine> get_muc_logs(const std::string& owner, const std::string& chan_name, const std::string& server,
                                              int limit=-1, const std::string& start="", const std::string& end="");
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
    return query.execute(Database::db);
  }

  static MucLogLineTable muc_log_lines;
  static GlobalOptionsTable global_options;
  static IrcServerOptionsTable irc_server_options;
  static IrcChannelOptionsTable irc_channel_options;
  static RosterTable roster;
  static sqlite3* db;

 private:
  static std::string gen_uuid();
};
#endif /* USE_DATABASE */
