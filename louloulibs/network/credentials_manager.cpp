#include "louloulibs.h"

#ifdef BOTAN_FOUND
#include <network/tcp_socket_handler.hpp>
#include <network/credentials_manager.hpp>
#include <logger/logger.hpp>
#include <botan/tls_exceptn.h>

#ifdef USE_DATABASE
# include <database/database.hpp>
#endif

Botan::Certificate_Store_In_Memory Basic_Credentials_Manager::certificate_store;
bool Basic_Credentials_Manager::certs_loaded = false;

Basic_Credentials_Manager::Basic_Credentials_Manager(const TCPSocketHandler* const socket_handler):
    Botan::Credentials_Manager(),
    socket_handler(socket_handler)
{
  this->load_certs();
}

void Basic_Credentials_Manager::verify_certificate_chain(const std::string& type,
                                                         const std::string& purported_hostname,
                                                         const std::vector<Botan::X509_Certificate>& certs)
{
  log_debug("Checking remote certificate (" << type << ") for hostname " << purported_hostname);
  try
    {
      Botan::Credentials_Manager::verify_certificate_chain(type, purported_hostname, certs);
      log_debug("Certificate is valid");
    }
  catch (const std::exception& tls_exception)
    {
      log_warning("TLS certificate check failed: " << tls_exception.what());
      if (this->socket_handler->abort_on_invalid_cert())
        throw;
    }
}

void Basic_Credentials_Manager::load_certs()
{
  //  Only load the certificates the first time
  if (Basic_Credentials_Manager::certs_loaded)
    return;
  const std::vector<std::string> paths = {"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem"};
  for (const auto& path: paths)
    {
      Botan::DataSource_Stream bundle(path);
      while (!bundle.end_of_data() && bundle.check_available(27))
        {
          const Botan::X509_Certificate cert(bundle);
          Basic_Credentials_Manager::certificate_store.add_certificate(cert);
        }
    }
  Basic_Credentials_Manager::certs_loaded = true;
}

std::vector<Botan::Certificate_Store*> Basic_Credentials_Manager::trusted_certificate_authorities(const std::string&, const std::string&)
{
  return {&this->certificate_store};
}

#endif
