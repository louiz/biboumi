#ifndef JID_INCLUDED
# define JID_INCLUDED

#include <string>

/**
 * Parse a JID into its different subart
 */
class Jid
{
public:
  explicit Jid(const std::string& jid);

  std::string domain;
  std::string local;
  std::string resource;

private:
  Jid(const Jid&) = delete;
  Jid(Jid&&) = delete;
  Jid& operator=(const Jid&) = delete;
  Jid& operator=(Jid&&) = delete;
};

/**
 * Prepare the given UTF-8 string according to the XMPP node stringprep
 * identifier profile.  This is used to send properly-formed JID to the XMPP
 * server.
 *
 * If the stringprep library is not found, we return an empty string.  When
 * this function is used, the result must always be checked for an empty
 * value, and if this is the case it must not be used as a JID.
 */
std::string jidprep(const std::string& original);

#endif // JID_INCLUDED
