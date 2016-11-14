#pragma once

#include <network/socket_handler.hpp>

#include <cassert>
#include <network/tcp_socket_handler.hpp>

#include <logger/logger.hpp>
#include <xmpp/biboumi_component.hpp>

class XmppComponent;
class IdentdSocket;
class IdentdServer;
template <typename T>
class TcpSocketServer;

class IdentdSocket: public TCPSocketHandler
{
 public:
  IdentdSocket(std::shared_ptr<Poller> poller, const socket_t socket, TcpSocketServer<IdentdSocket>& server);
  ~IdentdSocket() = default;
  std::string generate_answer(const BiboumiComponent& biboumi, uint16_t local, uint16_t remote);

  void parse_in_buffer(const std::size_t size) override final;
  void on_connection_close(const std::string& message) override final
  {}

  bool is_connected() const override final
  {
    return true;
  }
  bool is_connecting() const override final
  {
    return false;
  }

 private:
  void connect() override
  { assert(false); }
  void on_connection_failed(const std::string&) override final
  { assert(false); }

  IdentdServer& server;
};
