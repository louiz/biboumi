#pragma once

#include "biboumi.h"

#ifdef BOTAN_FOUND

#include <botan/botan.h>
#include <botan/tls_client.h>

class TCPSocketHandler;

/**
 * If the given cert isn’t valid, based on the given hostname
 * and fingerprint, then throws the exception if it’s non-empty.
 *
 * Must be called after the standard (from Botan) way of
 * checking the certificate, if we want to also accept certificates based
 * on a trusted fingerprint.
 */
void check_tls_certificate(const std::vector<Botan::X509_Certificate>& certs,
                           const std::string& hostname, const std::string& trusted_fingerprint,
                           std::exception_ptr exc);

class BasicCredentialsManager: public Botan::Credentials_Manager
{
public:
  BasicCredentialsManager(const TCPSocketHandler* const socket_handler);

  BasicCredentialsManager(BasicCredentialsManager&&) = delete;
  BasicCredentialsManager(const BasicCredentialsManager&) = delete;
  BasicCredentialsManager& operator=(const BasicCredentialsManager&) = delete;
  BasicCredentialsManager& operator=(BasicCredentialsManager&&) = delete;

#if BOTAN_VERSION_CODE < BOTAN_VERSION_CODE_FOR(1,11,34)
  void verify_certificate_chain(const std::string& type,
                                const std::string& purported_hostname,
                                const std::vector<Botan::X509_Certificate>&) override final;
#endif
  std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(const std::string& type,
                                                                         const std::string& context) override final;
  void set_trusted_fingerprint(const std::string& fingerprint);
  const std::string& get_trusted_fingerprint() const;

private:
  const TCPSocketHandler* const socket_handler;

  static bool try_to_open_one_ca_bundle(const std::vector<std::string>& paths);
  static void load_certs();
  static Botan::Certificate_Store_In_Memory certificate_store;
  static bool certs_loaded;
  std::string trusted_fingerprint;
};

#endif //BOTAN_FOUND

