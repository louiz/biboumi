#pragma once

#include <network/tcp_server_socket.hpp>
#include <identd/identd_socket.hpp>
#include <algorithm>
#include <unistd.h>

class BiboumiComponent;

class IdentdServer: public TcpSocketServer<IdentdSocket>
{
 public:
  IdentdServer(const BiboumiComponent& biboumi_component, std::shared_ptr<Poller>& poller, const uint16_t port):
      TcpSocketServer<IdentdSocket>(poller, port),
      biboumi_component(biboumi_component)
  {}

  const BiboumiComponent& get_biboumi_component() const
  {
    return this->biboumi_component;
  }
  void shutdown()
  {
    if (this->poller->is_managing_socket(this->socket))
      this->poller->remove_socket_handler(this->socket);
    ::close(this->socket);
  }
  void clean()
  {
    this->sockets.erase(std::remove_if(this->sockets.begin(), this->sockets.end(),
                                       [](const std::unique_ptr<IdentdSocket>& socket)
                                       {
                                         return socket->get_socket() == -1;
                                       }),
                        this->sockets.end());
  }
 private:
  const BiboumiComponent& biboumi_component;
};
