#include <xmpp/jid.hpp>
#include <config.h>
#include <cstring>
#include <map>

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
  using CacheType = std::map<std::string, std::string>;
  static CacheType cache;
  std::pair<CacheType::iterator, bool> cached = cache.insert({original, {}});
  if (std::get<1>(cached) == false)
    { // Insertion failed: the result is already in the cache, return it
      return std::get<0>(cached)->second;
    }

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
    {
      std::get<0>(cached)->second = std::string(local) + "@" + domain;
      return std::get<0>(cached)->second;
    }

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
  std::get<0>(cached)->second = std::string(local) + "@" + domain + "/" + resource;
  return std::get<0>(cached)->second;

#else
  (void)original;
  return "";
#endif
}
