#include <xmpp/jid.hpp>
#include <config.h>
#include <cstring>

#ifdef LIBIDN_FOUND
 #include <stringprep.h>
#endif

#include <logger/logger.hpp>

Jid::Jid(const std::string& jid)
{
  std::string::size_type slash = jid.find('/');
  if (slash != std::string::npos)
    {
      this->resource = jid.substr(slash + 1);
    }

  std::string::size_type at = jid.find('@');
  if (at != std::string::npos && at < slash)
    {
      this->local = jid.substr(0, at);
      at++;
    }
  else
    at = 0;

  this->domain = jid.substr(at, slash - at);
}

#include <iostream>

static constexpr size_t max_jid_part_len = 1023;

std::string jidprep(const std::string& original)
{
#ifdef LIBIDN_FOUND
  // TODO: cache the result
  const std::string error_msg("Failed to convert " + original + " into a valid JID:");
  Jid jid(original);

  char local[max_jid_part_len] = {};
  memcpy(local, jid.local.data(), jid.local.size());
  Stringprep_rc rc = static_cast<Stringprep_rc>(::stringprep(local, max_jid_part_len,
                     static_cast<Stringprep_profile_flags>(0), stringprep_xmpp_nodeprep));
  if (rc != STRINGPREP_OK)
  {
    log_error(error_msg + stringprep_strerror(rc));
    return "";
  }

  char domain[max_jid_part_len] = {};
  memcpy(domain, jid.domain.data(), jid.domain.size());
  rc = static_cast<Stringprep_rc>(::stringprep(domain, max_jid_part_len,
       static_cast<Stringprep_profile_flags>(0), stringprep_nameprep));
  if (rc != STRINGPREP_OK)
  {
    log_error(error_msg + stringprep_strerror(rc));
    return "";
  }

  // If there is no resource, stop here
  if (jid.resource.empty())
    return std::string(local) + "@" + domain;

  // Otherwise, also process the resource part
  char resource[max_jid_part_len] = {};
  memcpy(resource, jid.resource.data(), jid.resource.size());
  rc = static_cast<Stringprep_rc>(::stringprep(resource, max_jid_part_len,
       static_cast<Stringprep_profile_flags>(0), stringprep_xmpp_resourceprep));
  if (rc != STRINGPREP_OK)
    {
      log_error(error_msg + stringprep_strerror(rc));
      return "";
    }
  return std::string(local) + "@" + domain + "/" + resource;

#else
  (void)original;
  return "";
#endif
}
