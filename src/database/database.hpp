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

  struct Uuid: Column<std::string> { static constexpr auto name = "uuid_";
    static constexpr auto options = ""; };

  struct Owner: Column<std::string> { static constexpr auto name = "owner_";
    static constexpr auto options = ""; };

  struct IrcChanName: Column<std::string> { static constexpr auto name = "ircChanName_";
    static constexpr auto options = ""; };

  struct Channel: Column<std::string> { static constexpr auto name = "channel_";
    static constexpr auto options = ""; };

  struct IrcServerName: Column<std::string> { static constexpr auto name = "ircServerName_";
    static constexpr auto options = ""; };

  struct Server: Column<std::string> { static constexpr auto name = "server_";
    static constexpr auto options = ""; };

  struct Date: Column<time_point::rep> { static constexpr auto name = "date_";
    static constexpr auto options = ""; };

  struct Body: Column<std::string> { static constexpr auto name = "body_";
    static constexpr auto options = ""; };

  struct Nick: Column<std::string> { static constexpr auto name = "nick_";
    static constexpr auto options = ""; };

  struct Pass: Column<std::string> { static constexpr auto name = "pass_";
    static constexpr auto options = ""; };

  struct Ports: Column<std::string> { static constexpr auto name = "ports_";
    static constexpr auto options = "";
    Ports(): Column<std::string>("6667") {} };

  struct TlsPorts: Column<std::string> { static constexpr auto name = "tlsPorts_";
    static constexpr auto options = "";
    TlsPorts(): Column<std::string>("6697;6670") {} };

  struct Username: Column<std::string> { static constexpr auto name = "username_";
    static constexpr auto options = ""; };

  struct Realname: Column<std::string> { static constexpr auto name = "realname_";
    static constexpr auto options = ""; };

  struct AfterConnectionCommand: Column<std::string> { static constexpr auto name = "afterConnectionCommand_";
    static constexpr auto options = ""; };

  struct TrustedFingerprint: Column<std::string> { static constexpr auto name = "trustedFingerprint_";
    static constexpr auto options = ""; };

  struct EncodingOut: Column<std::string> { static constexpr auto name = "encodingOut_";
    static constexpr auto options = ""; };

  struct EncodingIn: Column<std::string> { static constexpr auto name = "encodingIn_";
    static constexpr auto options = ""; };

  struct MaxHistoryLength: Column<int> { static constexpr auto name = "maxHistoryLength_";
    static constexpr auto options = "";
    MaxHistoryLength(): Column<int>(20) {} };

  struct RecordHistory: Column<bool> { static constexpr auto name = "recordHistory_";
    static constexpr auto options = "";
    RecordHistory(): Column<bool>(true) {}};

  struct RecordHistoryOptional: Column<OptionalBool> { static constexpr auto name = "recordHistory_";
    static constexpr auto options = ""; };

  struct VerifyCert: Column<bool> { static constexpr auto name = "verifyCert_";
    static constexpr auto options = "";
    VerifyCert(): Column<bool>(true) {} };

  struct Persistent: Column<bool> { static constexpr auto name = "persistent_";
    static constexpr auto options = "";
    Persistent(): Column<bool>(false) {} };

  using MucLogLineTable = Table<Id, Uuid, Owner, IrcChanName, IrcServerName, Date, Body, Nick>;
  using MucLogLine = MucLogLineTable::RowType;

  using GlobalOptionsTable = Table<Id, Owner, MaxHistoryLength, RecordHistory>;
  using GlobalOptions = GlobalOptionsTable::RowType;

  using IrcServerOptionsTable = Table<Id, Owner, Server, Pass, AfterConnectionCommand, TlsPorts, Ports, Username, Realname, VerifyCert, TrustedFingerprint, EncodingOut, EncodingIn, MaxHistoryLength>;
  using IrcServerOptions = IrcServerOptionsTable::RowType;

  using IrcChannelOptionsTable = Table<Id, Owner, Server, Channel, EncodingOut, EncodingIn, MaxHistoryLength, Persistent, RecordHistoryOptional>;
  using IrcChannelOptions = IrcChannelOptionsTable::RowType;

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
  static sqlite3* db;

 private:
  static std::string gen_uuid();
};
#endif /* USE_DATABASE */
