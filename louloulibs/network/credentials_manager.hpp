#ifndef BIBOUMI_CREDENTIALS_MANAGER_HPP
#define BIBOUMI_CREDENTIALS_MANAGER_HPP

#include "louloulibs.h"

#ifdef BOTAN_FOUND

#include <botan/botan.h>
#include <botan/tls_client.h>

class TCPSocketHandler;

class BasicCredentialsManager: public Botan::Credentials_Manager
{
public:
  BasicCredentialsManager(const TCPSocketHandler* const socket_handler);

  BasicCredentialsManager(BasicCredentialsManager&&) = delete;
  BasicCredentialsManager(const BasicCredentialsManager&) = delete;
  BasicCredentialsManager& operator=(const BasicCredentialsManager&) = delete;
  BasicCredentialsManager& operator=(BasicCredentialsManager&&) = delete;

  void verify_certificate_chain(const std::string& type,
                                const std::string& purported_hostname,
                                const std::vector<Botan::X509_Certificate>&) override final;
  std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(const std::string& type,
                                                                         const std::string& context) override final;
  void set_trusted_fingerprint(const std::string& fingerprint);

private:
  const TCPSocketHandler* const socket_handler;

  static void load_certs();
  static Botan::Certificate_Store_In_Memory certificate_store;
  static bool certs_loaded;
  std::string trusted_fingerprint;
};

#endif //BOTAN_FOUND
#endif //BIBOUMI_CREDENTIALS_MANAGER_HPP
