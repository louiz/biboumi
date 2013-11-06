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


#endif // JID_INCLUDED
