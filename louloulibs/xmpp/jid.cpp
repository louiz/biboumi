#include <xmpp/jid.hpp>
#include <algorithm>
#include <cstring>
#include <map>

#include <louloulibs.h>
#ifdef LIBIDN_FOUND
 #include <stringprep.h>
 #include <sys/types.h>
 #include <sys/socket.h>
 #include <netdb.h>
 #include <utils/scopeguard.hpp>
 #include <set>
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
  memcpy(local, jid.local.data(), std::min(max_jid_part_len, jid.local.size()));
  Stringprep_rc rc = static_cast<Stringprep_rc>(::stringprep(local, max_jid_part_len,
                     static_cast<Stringprep_profile_flags>(0), stringprep_xmpp_nodeprep));
  if (rc != STRINGPREP_OK)
  {
    log_error(error_msg + stringprep_strerror(rc));
    return "";
  }

  char domain[max_jid_part_len] = {};
  memcpy(domain, jid.domain.data(), std::min(max_jid_part_len, jid.domain.size()));

  {
    // Using getaddrinfo, check if the domain part is a valid IPv4 (then use
    // it as is), or IPv6 (surround it with []), or a domain name (run
    // nameprep)
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    struct addrinfo* addr_res = nullptr;
    const auto ret = ::getaddrinfo(domain, nullptr, &hints, &addr_res);
    auto addrinfo_deleter = utils::make_scope_guard([addr_res] { freeaddrinfo(addr_res); });
    if (ret || !addr_res || (addr_res->ai_family != AF_INET && addr_res->ai_family != AF_INET6))
      { // Not an IP, run nameprep on it
        rc = static_cast<Stringprep_rc>(::stringprep(domain, max_jid_part_len,
                                                     static_cast<Stringprep_profile_flags>(0), stringprep_nameprep));
        if (rc != STRINGPREP_OK)
          {
            log_error(error_msg + stringprep_strerror(rc));
            return "";
          }

        // Make sure it contains only allowed characters
        using std::begin;
        using std::end;
        char* domain_end = domain + ::strlen(domain);
        std::replace_if(std::begin(domain), domain + ::strlen(domain),
                        [](const char c) -> bool
                        {
                          return !((c >= 'a' && c <= 'z') || c == '-' ||
                                   (c >= '0' && c <= '9') || c == '.');
                        }, '-');
        // Make sure there are no doubled - or .
        std::set<char> special_chars{'-', '.'};
        domain_end = std::unique(begin(domain), domain + ::strlen(domain), [&special_chars](const char& a, const char& b) -> bool
        {
          return special_chars.count(a) && special_chars.count(b);
        });
        // remove leading and trailing -. if any
        if (domain_end != domain && special_chars.count(*(domain_end - 1)))
          --domain_end;
        if (domain_end != domain && special_chars.count(domain[0]))
          {
            std::memmove(domain, domain + 1, domain_end - domain + 1);
            --domain_end;
          }
        // And if the final result is an empty string, return a dummy hostname
        if (domain_end == domain)
          ::strcpy(domain, "empty");
        else
          *domain_end = '\0';
      }
    else if (addr_res->ai_family == AF_INET6)
      { // IPv6, surround it with []. The length is always enough:
        // the longest possible IPv6 is way shorter than max_jid_part_len
        ::memmove(domain + 1, domain, jid.domain.size());
        domain[0] = '[';
        domain[jid.domain.size() + 1] = ']';
      }
  }


  // If there is no resource, stop here
  if (jid.resource.empty())
    {
      std::get<0>(cached)->second = std::string(local) + "@" + domain;
      return std::get<0>(cached)->second;
    }

  // Otherwise, also process the resource part
  char resource[max_jid_part_len] = {};
  memcpy(resource, jid.resource.data(), std::min(max_jid_part_len, jid.resource.size()));
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
