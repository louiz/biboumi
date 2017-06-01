#pragma once

#include <database/common.hpp>

#include <odb/core.hxx>

#include <string>

/**
 * Navite views, only used to query the data from the old tables,
 * in order to migrate them in the new schema.
 *
 * This follows the schema from biboumi 5.0 only.
 * Data migration from previous versions is not supported.
 */
namespace old_database
{

#pragma db view query("SELECT * FROM MucLogLine_")
struct MucLogLine
{
  int id;
  std::string type;
  std::string uuid;
  std::string owner;
  std::string channel_name;
  std::string server_name;
  Timestamp date{};
  std::string body;
  std::string nick;
};

#pragma db view query("SELECT * FROM GlobalOptions_")
struct GlobalOptions
{
  int id;
  std::string type;
  std::string owner;
  int max_history_length;
  int record_history{false};
};

#pragma db view query("SELECT * from IrcServerOptions_")
struct IrcServerOptions
{
  int id;
  std::string type;
  std::string owner;
  std::string server;
  std::string pass;
  std::string after_connection_command;
  std::string tls_ports;
  std::string ports;
  std::string username;
  std::string realname;
  bool verify_cert{true};
  std::string trusted_fingerprint;
  std::string encoding_out;
  std::string encoding_in;
  int max_history_length{20};
};

#pragma db view query("SELECT * from IrcChannelOptions_")
struct IrcChannelOptions
{
  int id;
  std::string type;
  std::string owner;
  std::string server;
  std::string channel;
  std::string encoding_out;
  std::string encoding_in;
  int max_history_length;
  bool persistent{false};
};

}