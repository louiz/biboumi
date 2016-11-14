#pragma once

#include <network/tcp_socket_handler.hpp>

class TCPClientSocketHandler: public TCPSocketHandler
{
 public:
  TCPClientSocketHandler(std::shared_ptr<Poller> poller);
  ~TCPClientSocketHandler();
  /**
   * Connect to the remote server, and call on_connected() if this
   * succeeds. If tls is true, we set use_tls to true and will also call
   * start_tls() when the connection succeeds.
   */
  void connect(const std::string& address, const std::string& port, const bool tls);
  void connect() override final;
  /**
   * Called by a TimedEvent, when the connection did not succeed or fail
   * after a given time.
   */
  void on_connection_timeout();
  /**
   * Called when the connection is successful.
   */
  virtual void on_connected() = 0;
  bool is_connected() const override;
  bool is_connecting() const;

  std::string get_port() const;

  void close() override final;
  std::chrono::system_clock::time_point connection_date;

  /**
   * Whether or not this connection is using the two given TCP ports.
   */
  bool match_port_pairt(const uint16_t local, const uint16_t remote) const;

 protected:
  bool hostname_resolution_failed;
  /**
   * Address to bind the socket to, before calling connect().
   * If empty, it’s equivalent to binding to INADDR_ANY.
   */
  std::string bind_addr;
  /**
   * Display the resolved IP, just for information purpose.
   */
  void display_resolved_ip(struct addrinfo* rp) const;
 private:
  /**
   * Initialize the socket with the parameters contained in the given
   * addrinfo structure.
   */
  void init_socket(const struct addrinfo* rp);
  /**
   * DNS resolver
   */
  Resolver resolver;
  /**
   * Keep the details of the addrinfo returned by the resolver that
   * triggered a EINPROGRESS error when connect()ing to it, to reuse it
   * directly when connect() is called again.
   */
  struct addrinfo addrinfo;
  struct sockaddr_in6 ai_addr;
  socklen_t ai_addrlen;

  /**
   * Hostname we are connected/connecting to
   */
  std::string address;
  /**
   * Port we are connected/connecting to
   */
  std::string port;

  uint16_t local_port;

  bool connected;
  bool connecting;
};
