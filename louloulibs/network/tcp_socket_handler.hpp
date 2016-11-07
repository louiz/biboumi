#pragma once


#include "louloulibs.h"

#include <network/socket_handler.hpp>
#include <network/resolver.hpp>

#include <network/credentials_manager.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <vector>
#include <memory>
#include <string>
#include <list>

/**
 * An interface, with a series of callbacks that should be implemented in
 * subclasses that deal with a socket. These callbacks are called on various events
 * (read/write/timeout, etc) when they are notified to a poller
 * (select/poll/epoll etc)
 */
class TCPSocketHandler: public SocketHandler
{
protected:
  ~TCPSocketHandler();
public:
  explicit TCPSocketHandler(std::shared_ptr<Poller> poller);
  TCPSocketHandler(const TCPSocketHandler&) = delete;
  TCPSocketHandler(TCPSocketHandler&&) = delete;
  TCPSocketHandler& operator=(const TCPSocketHandler&) = delete;
  TCPSocketHandler& operator=(TCPSocketHandler&&) = delete;

  /**
   * Connect to the remote server, and call on_connected() if this
   * succeeds. If tls is true, we set use_tls to true and will also call
   * start_tls() when the connection succeeds.
   */
  void connect(const std::string& address, const std::string& port, const bool tls);
  void connect() override final;
  /**
   * Reads raw data from the socket. And pass it to parse_in_buffer()
   * If we are using TLS on this connection, we call tls_recv()
   */
  void on_recv() override final;
  /**
   * Write as much data from out_buf as possible, in the socket.
   */
  void on_send() override final;
  /**
   * Add the given data to out_buf and tell our poller that we want to be
   * notified when a send event is ready.
   *
   * This can be overriden if we want to modify the data before sending
   * it. For example if we want to encrypt it.
   */
  void send_data(std::string&& data);
  /**
   * Watch the socket for send events, if our out buffer is not empty.
   */
  void send_pending_data();
  /**
   * Close the connection, remove us from the poller
   */
  void close();
  /**
   * Called by a TimedEvent, when the connection did not succeed or fail
   * after a given time.
   */
  void on_connection_timeout();
  /**
   * Called when the connection is successful.
   */
  virtual void on_connected() = 0;
  /**
   * Called when the connection fails. Not when it is closed later, just at
   * the connect() call.
   */
  virtual void on_connection_failed(const std::string& reason) = 0;
  /**
   * Called when we detect a disconnection from the remote host.
   */
  virtual void on_connection_close(const std::string& error) = 0;
  /**
   * Handle/consume (some of) the data received so far.  The data to handle
   * may be in the in_buf buffer, or somewhere else, depending on what
   * get_receive_buffer() returned.  If some data is used from in_buf, it
   * should be truncated, only the unused data should be left untouched.
   *
   * The size argument is the size of the last chunk of data that was added to the buffer.
   */
  virtual void parse_in_buffer(const size_t size) = 0;
#ifdef BOTAN_FOUND
  /**
   * Tell whether the credential manager should cancel the connection when the
   * certificate is invalid.
   */
  virtual bool abort_on_invalid_cert() const
  {
    return true;
  }
#endif
  bool is_connected() const override final;
  bool is_connecting() const;
  bool is_using_tls() const;
  std::string get_port() const;
  std::chrono::system_clock::time_point connection_date;

private:
  /**
   * Initialize the socket with the parameters contained in the given
   * addrinfo structure.
   */
  void init_socket(const struct addrinfo* rp);
  /**
   * Reads from the socket into the provided buffer.  If an error occurs
   * (read returns <= 0), the handling of the error is done here (close the
   * connection, log a message, etc).
   *
   * Returns the value returned by ::recv(), so the buffer should not be
   * used if it’s not positive.
   */
  ssize_t do_recv(void* recv_buf, const size_t buf_size);
  /**
   * Reads data from the socket and calls parse_in_buffer with it.
   */
  void plain_recv();
  /**
   * Mark the given data as ready to be sent, as-is, on the socket, as soon
   * as we can.
   */
  void raw_send(std::string&& data);

#ifdef BOTAN_FOUND
  /**
   * Create the TLS::Client object, with all the callbacks etc. This must be
   * called only when we know we are able to send TLS-encrypted data over
   * the socket.
   */
  void start_tls();
  /**
   * An additional step to pass the data into our tls object to decrypt it
   * before passing it to parse_in_buffer.
   */
  void tls_recv();
  /**
   * Pass the data to the tls object in order to encrypt it. The tls object
   * will then call raw_send as a callback whenever data as been encrypted
   * and can be sent on the socket.
   */
  void tls_send(std::string&& data);
  /**
   * Called by the tls object that some data has been decrypt. We call
   * parse_in_buffer() to handle that unencrypted data.
   */
  void tls_data_cb(const Botan::byte* data, size_t size);
  /**
   * Called by the tls object to indicate that some data has been encrypted
   * and is now ready to be sent on the socket as is.
   */
  void tls_output_fn(const Botan::byte* data, size_t size);
  /**
   * Called by the tls object to indicate that a TLS alert has been
   * received. We don’t use it, we just log some message, at the moment.
   */
  void tls_alert_cb(Botan::TLS::Alert alert, const Botan::byte*, size_t);
  /**
   * Called by the tls object at the end of the TLS handshake. We don't do
   * anything here appart from logging the TLS session information.
   */
  bool tls_handshake_cb(const Botan::TLS::Session& session);
  /**
   * Called whenever the tls session goes from inactive to active. This
   * means that the handshake has just been successfully done, and we can
   * now proceed to send any available data into our tls object.
   */
  void on_tls_activated();
#endif // BOTAN_FOUND
  /**
   * Where data is added, when we want to send something to the client.
   */
  std::vector<std::string> out_buf;
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

protected:
  /**
   * Where data read from the socket is added until we can extract a full
   * and meaningful “message” from it.
   *
   * TODO: something more efficient than a string.
   */
  std::string in_buf;
  /**
   * Whether we are using TLS on this connection or not.
   */
  bool use_tls;
  /**
   * Provide a buffer in which data can be directly received. This can be
   * used to avoid copying data into in_buf before using it. If no buffer
   * needs to be provided, nullptr is returned (the default implementation
   * does that), in that case our internal in_buf will be used to save the
   * data until it can be used by parse_in_buffer().
   */
  virtual void* get_receive_buffer(const size_t size) const;
  /**
   * Hostname we are connected/connecting to
   */
  std::string address;
  /**
   * Port we are connected/connecting to
   */
  std::string port;

  bool connected;
  bool connecting;

  bool hostname_resolution_failed;

  /**
   * Address to bind the socket to, before calling connect().
   * If empty, it’s equivalent to binding to INADDR_ANY.
   */
  std::string bind_addr;

private:
  /**
   * Display the resolved IP, just for information purpose.
   */
  void display_resolved_ip(struct addrinfo* rp) const;

#ifdef BOTAN_FOUND
  /**
   * Botan stuff to manipulate a TLS session.
   */
  static Botan::AutoSeeded_RNG rng;
  static Botan::TLS::Policy policy;
  static Botan::TLS::Session_Manager_In_Memory session_manager;
protected:
  BasicCredentialsManager credential_manager;
private:
  /**
   * We use a unique_ptr because we may not want to create the object at
   * all. The Botan::TLS::Client object generates a handshake message and
   * calls the output_fn callback with it as soon as it is created.
   * Therefore, we do not want to create it if we do not intend to send any
   * TLS-encrypted message. We create the object only when needed (for
   * example after we have negociated a TLS session using a STARTTLS
   * message, or stuf like that).
   *
   * See start_tls for the method where this object is created.
   */
  std::unique_ptr<Botan::TLS::Client> tls;
  /**
   * An additional buffer to keep data that the user wants to send, but
   * cannot because the handshake is not done.
   */
  std::vector<Botan::byte> pre_buf;
#endif // BOTAN_FOUND
};
