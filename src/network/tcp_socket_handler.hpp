#ifndef SOCKET_HANDLER_INCLUDED
# define SOCKET_HANDLER_INCLUDED

#include <network/socket_handler.hpp>

#include <logger/logger.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <utility>
#include <memory>
#include <string>
#include <list>

#include "config.h"

#ifdef CARES_FOUND
# include <ares.h>
#endif

#ifdef BOTAN_FOUND
# include <botan/botan.h>
# include <botan/tls_client.h>

/**
 * A very simple credential manager that accepts any certificate.
 */
class Permissive_Credentials_Manager: public Botan::Credentials_Manager
{
public:
  void verify_certificate_chain(const std::string& type, const std::string& purported_hostname, const std::vector<Botan::X509_Certificate>&)
  { // TODO: Offer the admin to disallow connection on untrusted
    // certificates
    log_debug("Checking remote certificate (" << type << ") for hostname " << purported_hostname);
  }
};
#endif // BOTAN_FOUND

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
  bool is_connected() const override final;
  bool is_connecting() const;

#ifdef CARES_FOUND
  void on_hostname4_resolved(int status, struct hostent* hostent);
  void on_hostname6_resolved(int status, struct hostent* hostent);

  void free_cares_addrinfo();

  void fill_ares_addrinfo4(const struct hostent* hostent);
  void fill_ares_addrinfo6(const struct hostent* hostent);
#endif

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
  std::list<std::string> out_buf;
  /**
   * Keep the details of the addrinfo that triggered a EINPROGRESS error when
   * connect()ing to it, to reuse it directly when connect() is called
   * again.
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

#ifdef CARES_FOUND
  /**
   * Whether or not the DNS resolution was successfully done
   */
  bool resolved;
  bool resolved4;
  bool resolved6;
  /**
   * When using c-ares to resolve the host asynchronously, we need the
   * c-ares callback to fill a structure (a struct addrinfo, for
   * compatibility with getaddrinfo and the rest of the code that works when
   * c-ares is not used) with all returned values (for example an IPv6 and
   * an IPv4). The next call of connect() will then try all these values
   * (exactly like we do with the result of getaddrinfo) and save the one
   * that worked (or returned EINPROGRESS) in the other struct addrinfo (see
   * the members addrinfo, ai_addrlen, and ai_addr).
   */
  struct addrinfo* cares_addrinfo;
  std::string cares_error;
#endif  // CARES_FOUND

private:
  TCPSocketHandler(const TCPSocketHandler&) = delete;
  TCPSocketHandler(TCPSocketHandler&&) = delete;
  TCPSocketHandler& operator=(const TCPSocketHandler&) = delete;
  TCPSocketHandler& operator=(TCPSocketHandler&&) = delete;

#ifdef BOTAN_FOUND
  /**
   * Botan stuff to manipulate a TLS session.
   */
  static Botan::AutoSeeded_RNG rng;
  static Permissive_Credentials_Manager credential_manager;
  static Botan::TLS::Policy policy;
  static Botan::TLS::Session_Manager_In_Memory session_manager;
  /**
   * We use a unique_ptr because we may not want to create the object at
   * all. The Botan::TLS::Client object generates a handshake message as
   * soon and calls the output_fn callback with it as soon as it is
   * created. Therefore, we do not want to create it if do not intend to do
   * send any TLS-encrypted message. We create the object only when needed
   * (for example after we have negociated a TLS session using a STARTTLS
   * message, or stuf like that).
   *
   * See start_tls for the method where this object is created.
   */
  std::unique_ptr<Botan::TLS::Client> tls;
  /**
   * An additional buffer to keep data that the user wants to send, but
   * cannot because the handshake is not done.
   */
  std::string pre_buf;
#endif // BOTAN_FOUND
};

#endif // SOCKET_HANDLER_INCLUDED
