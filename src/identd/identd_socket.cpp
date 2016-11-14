#include <identd/identd_socket.hpp>
#include <identd/identd_server.hpp>
#include <xmpp/biboumi_component.hpp>
#include <iomanip>

#include <utils/sha1.hpp>

#include <logger/logger.hpp>

IdentdSocket::IdentdSocket(std::shared_ptr<Poller> poller, const socket_t socket, TcpSocketServer<IdentdSocket>& server):
  TCPSocketHandler(poller),
  server(dynamic_cast<IdentdServer&>(server))
{
  this->socket = socket;
}

void IdentdSocket::parse_in_buffer(const std::size_t)
{
  while (true)
    {
      const auto line_end = this->in_buf.find('\n');
      if (line_end == std::string::npos)
        break;
      std::istringstream line(this->in_buf.substr(0, line_end));
      this->consume_in_buffer(line_end + 1);

      uint16_t local_port;
      uint16_t remote_port;
      char sep;
      line >> local_port >> sep >> remote_port;
      const auto& xmpp = this->server.get_biboumi_component();
      auto response = this->generate_answer(xmpp, local_port, remote_port);

      this->send_data(std::move(response));
    }
}

static std::string hash_jid(const std::string& jid)
{
  sha1nfo sha1;
  sha1_init(&sha1);
  sha1_write(&sha1, jid.data(), jid.size());
  const uint8_t* res = sha1_result(&sha1);
  std::ostringstream result;
  for (int i = 0; i < HASH_LENGTH; i++)
    result << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(res[i]);
  return result.str();
}

std::string IdentdSocket::generate_answer(const BiboumiComponent& biboumi, uint16_t local, uint16_t remote)
{
  for (const Bridge* bridge: biboumi.get_bridges())
    {
      for (const auto& pair: bridge->get_irc_clients())
        {
          if (pair.second->match_port_pairt(local, remote))
            {
              std::ostringstream os;
              os << local << " , " << remote << " : USERID : OTHER : " << hash_jid(bridge->get_bare_jid());
              log_debug("Identd, sending: ", os.str());
              return os.str();
            }
        }
    }
  std::ostringstream os;
  os << local << " , " << remote << " ERROR : NO-USER";
  log_debug("Identd, sending: ", os.str());
  return os.str();
}
