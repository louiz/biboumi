#pragma once

#include "louloulibs.h"

#include <network/socket_handler.hpp>
#include <network/resolver.hpp>

#include <network/credentials_manager.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <chrono>
#include <vector>
#include <memory>
#include <string>
#include <list>

#ifdef BOTAN_FOUND

# include <botan/types.h>
# include <botan/botan.h>
# include <botan/tls_session_manager.h>

class BiboumiTLSPolicy: public Botan::TLS::Policy
{
public:
# if BOTAN_VERSION_CODE >= BOTAN_VERSION_CODE_FOR(1,11,33)
  bool use_ecc_point_compression() const override
  {
    return true;
  }
# endif
};

# if BOTAN_VERSION_CODE >= BOTAN_VERSION_CODE_FOR(1,11,32)
#  define BOTAN_TLS_CALLBACKS_OVERRIDE override final
# else
#  define BOTAN_TLS_CALLBACKS_OVERRIDE
# endif
#endif

/**
 * Does all the read/write, buffering etc. With optional tls.
 * But doesn’t do any connect() or accept() or anything else.
 */
class TCPSocketHandler: public SocketHandler
#ifdef BOTAN_FOUND
# if BOTAN_VERSION_CODE >= BOTAN_VERSION_CODE_FOR(1,11,32)
    ,public Botan::TLS::Callbacks
# endif
#endif
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
  virtual void close();
  /**
   * Handle/consume (some of) the data received so far.  The data to handle
   * may be in the in_buf buffer, or somewhere else, depending on what
   * get_receive_buffer() returned.  If some data is used from in_buf, it
   * should be truncated, only the unused data should be left untouched.
   *
   * The size argument is the size of the last chunk of data that was added to the buffer.
   *
   * The function should call consume_in_buffer, with the size that was consumed by the
   * “parsing”, and thus to be removed from the input buffer.
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
  bool is_using_tls() const;

private:
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

 protected:
  virtual bool is_connecting() const = 0;
#ifdef BOTAN_FOUND
  /**
   * Create the TLS::Client object, with all the callbacks etc. This must be
   * called only when we know we are able to send TLS-encrypted data over
   * the socket.
   */
  void start_tls(const std::string& address, const std::string& port);
 private:
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
  void tls_record_received(uint64_t rec_no, const Botan::byte* data, size_t size) BOTAN_TLS_CALLBACKS_OVERRIDE;
  /**
   * Called by the tls object to indicate that some data has been encrypted
   * and is now ready to be sent on the socket as is.
   */
  void tls_emit_data(const Botan::byte* data, size_t size) BOTAN_TLS_CALLBACKS_OVERRIDE;
  /**
   * Called by the tls object to indicate that a TLS alert has been
   * received. We don’t use it, we just log some message, at the moment.
   */
  void tls_alert(Botan::TLS::Alert alert) BOTAN_TLS_CALLBACKS_OVERRIDE;
  /**
   * Called by the tls object at the end of the TLS handshake. We don't do
   * anything here appart from logging the TLS session information.
   */
  bool tls_session_established(const Botan::TLS::Session& session) BOTAN_TLS_CALLBACKS_OVERRIDE;
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
protected:
  /**
   * Whether we are using TLS on this connection or not.
   */
  bool use_tls;
  /**
   * Where data read from the socket is added until we can extract a full
   * and meaningful “message” from it.
   *
   * TODO: something more efficient than a string.
   */
  std::string in_buf;
  /**
   * Remove the given “size” first bytes from our in_buf.
   */
  void consume_in_buffer(const std::size_t size);
  /**
   * Provide a buffer in which data can be directly received. This can be
   * used to avoid copying data into in_buf before using it. If no buffer
   * needs to be provided, nullptr is returned (the default implementation
   * does that), in that case our internal in_buf will be used to save the
   * data until it can be used by parse_in_buffer().
   */
  virtual void* get_receive_buffer(const size_t size) const;
  /**
   * Called when we detect a disconnection from the remote host.
   */
  virtual void on_connection_close(const std::string&) {}
  virtual void on_connection_failed(const std::string&) {}

private:
#ifdef BOTAN_FOUND
  /**
   * Botan stuff to manipulate a TLS session.
   */
  static Botan::AutoSeeded_RNG rng;
  static BiboumiTLSPolicy policy;
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
