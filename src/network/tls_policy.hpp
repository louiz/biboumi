#pragma once

#include "biboumi.h"

#ifdef BOTAN_FOUND

#include <botan/tls_policy.h>

class BiboumiTLSPolicy: public Botan::TLS::Text_Policy
{
public:
  BiboumiTLSPolicy():
      Botan::TLS::Text_Policy({})
  {}
  bool load(const std::string& filename);
  void load(std::istream& iss);

  BiboumiTLSPolicy(const BiboumiTLSPolicy &) = delete;
  BiboumiTLSPolicy(BiboumiTLSPolicy &&) = delete;
  BiboumiTLSPolicy &operator=(const BiboumiTLSPolicy &) = delete;
  BiboumiTLSPolicy &operator=(BiboumiTLSPolicy &&) = delete;

  bool require_cert_revocation_info() const override;
  bool verify_certificate_info() const;
protected:
  bool req_cert_revocation_info{true};
  bool verify_certificate{true};
};

#endif
