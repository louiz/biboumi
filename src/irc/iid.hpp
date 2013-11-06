#ifndef IID_INCLUDED
# define IID_INCLUDED

#include <string>

/**
 * A name representing an IRC channel, on the same model than the XMPP JIDs (but much simpler).
 * The separator between the server and the channel name is '%'
 * #test%irc.freenode.org has :
 * - chan: "#test" (the # is part of the name, it could very well be absent, or & instead
 * - server: "irc.freenode.org"
 * #test has:
 * - chan: "#test"
 * - server: ""
 * %irc.freenode.org:
 * - chan: ""
 * - server: "irc.freenode.org"
 */
class Iid
{
public:
  explicit Iid(const std::string& iid);

  std::string chan;
  std::string server;

private:
  Iid(const Iid&) = delete;
  Iid(Iid&&) = delete;
  Iid& operator=(const Iid&) = delete;
  Iid& operator=(Iid&&) = delete;
};

#endif // IID_INCLUDED
