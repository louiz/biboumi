#include <network/credentials_manager.hpp>
#include <logger/logger.hpp>

Basic_Credentials_Manager::Basic_Credentials_Manager():
    Botan::Credentials_Manager()
{
  this->load_certs();
}
void Basic_Credentials_Manager::verify_certificate_chain(const std::string& type,
                                                         const std::string& purported_hostname,
                                                         const std::vector<Botan::X509_Certificate>& certs)
{
  log_debug("Checking remote certificate (" << type << ") for hostname " << purported_hostname);
  Botan::Credentials_Manager::verify_certificate_chain(type, purported_hostname, certs);
  log_debug("Certificate is valid");
}
void Basic_Credentials_Manager::load_certs()
{
  const std::vector<std::string> paths = {"/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem"};
  for (const auto& path: paths)
    {
      Botan::DataSource_Stream bundle(path);
      while (!bundle.end_of_data() && bundle.check_available(27))
        {
          const Botan::X509_Certificate cert(bundle);
          this->certificate_store.add_certificate(cert);
        }
    }
}
std::vector<Botan::Certificate_Store*> Basic_Credentials_Manager::trusted_certificate_authorities(const std::string&, const std::string&)
{
  return {&this->certificate_store};
}
