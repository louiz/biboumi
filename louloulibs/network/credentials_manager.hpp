#ifndef BIBOUMI_CREDENTIALS_MANAGER_HPP
#define BIBOUMI_CREDENTIALS_MANAGER_HPP

#include "louloulibs.h"

#ifdef BOTAN_FOUND

#include <botan/botan.h>
#include <botan/tls_client.h>

class TCPSocketHandler;

class Basic_Credentials_Manager: public Botan::Credentials_Manager
{
public:
  Basic_Credentials_Manager(const TCPSocketHandler* const socket_handler);
  void verify_certificate_chain(const std::string& type,
                                const std::string& purported_hostname,
                                const std::vector<Botan::X509_Certificate>&) override final;
  std::vector<Botan::Certificate_Store*> trusted_certificate_authorities(const std::string& type,
                                                                         const std::string& context) override final;

private:
  const TCPSocketHandler* const socket_handler;

  static void load_certs();
  static Botan::Certificate_Store_In_Memory certificate_store;
  static bool certs_loaded;
};

#endif //BOTAN_FOUND
#endif //BIBOUMI_CREDENTIALS_MANAGER_HPP
