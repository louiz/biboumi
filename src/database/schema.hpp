#pragma once

#include <database/common.hpp>
#include <database/old_schema.hpp>

#include <utils/split.hpp>

#include <odb/core.hxx>

#include <algorithm>
#include <vector>
#include <string>

#pragma db model version(1, 1)

namespace database
{

#pragma db object
struct MucLogLine
{
  MucLogLine() = default;
  MucLogLine(const old_database::MucLogLine old):
      uuid(old.uuid), owner(old.owner), channel_name(old.channel_name),
      server_name(old.server_name), body(old.body), nick(old.nick), date(old.date)
      {}

  #pragma db index member(uuid)
  #pragma db id
  std::string uuid;
  std::string owner;
  std::string channel_name;
  std::string server_name;
  std::string body;
  std::string nick;
  Timestamp date{};
};

#pragma db object
struct GlobalOptions
{
  GlobalOptions() = default;
  GlobalOptions(const old_database::GlobalOptions& old):
      owner(old.owner), max_history_length(old.max_history_length), record_history(old.record_history)
  {}

  #pragma db index unique member(owner)
  #pragma db id
  std::string owner;
  int max_history_length{20};
  bool record_history{true};
};

#pragma db object
struct IrcServerOptions
{
  IrcServerOptions() = default;
  IrcServerOptions(const old_database::IrcServerOptions& old):
      owner(old.owner), server(old.server), pass(old.pass), after_connection_command(old.after_connection_command),
      username(old.username), realname(old.realname),
      verify_cert(old.verify_cert), trusted_fingerprint(old.trusted_fingerprint), encoding_out(old.encoding_out),
      encoding_in(old.encoding_in), max_history_length(old.max_history_length)
  {
    {
      auto tls_ports = utils::split(old.tls_ports, ':', false);
      std::transform(std::begin(tls_ports), std::end(tls_ports),
                     std::back_inserter(this->tls_ports),
                     [](const std::string &port)
                     { return atoi(port.data()); });
    }
    {
      auto ports = utils::split(old.ports, ':', false);
      std::transform(std::begin(ports), std::end(ports),
                     std::back_inserter(this->ports),
                     [](const std::string &port)
                     { return atoi(port.data()); });
    }
  }

  #pragma db id auto
  uint64_t id;
  #pragma db index("owner_server") unique members(owner, server)
  std::string owner;
  std::string server;
  std::string pass;
  std::string after_connection_command;
  std::vector<uint16_t> tls_ports;
  std::vector<uint16_t> ports;
  std::string username;
  std::string realname;
  bool verify_cert;
  std::string trusted_fingerprint;
  std::string encoding_out;
  std::string encoding_in;
  int max_history_length{20};
};

#pragma db object
struct IrcChannelOptions
{
  IrcChannelOptions() = default;

  IrcChannelOptions(const old_database::IrcChannelOptions& old):
      owner(old.owner), server(old.server), channel(old.channel),
      encoding_out(old.encoding_out), encoding_in(old.encoding_in),
      max_history_length(old.max_history_length), persistent(old.persistent)
  {}

  #pragma db id auto
  int id;
  #pragma db index("owner_server_channel") unique members(owner, server, channel)
  std::string owner;
  std::string server;
  std::string channel;
  std::string encoding_out;
  std::string encoding_in;
  int max_history_length;
  bool persistent;
};

}
