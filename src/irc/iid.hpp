#ifndef IID_INCLUDED
# define IID_INCLUDED

#include <string>

/**
 * A name representing an IRC channel on an IRC server, or an IRC user on an
 * IRC server, or just an IRC server.
 *
 * The separator for an user is '!', for a channel it's '%'. If no separator
 * is present, it's just an irc server.
 * It’s possible to have an empty-string server, but it makes no sense in
 * the biboumi context.
 *
 * #test%irc.example.org has :
 * - local: "#test" (the # is part of the name, it could very well be absent, or & (for example) instead)
 * - server: "irc.example.org"
 * - is_channel: true
 * - is_user: false
 *
 * %irc.example.org:
 * - local: ""
 * - server: "irc.example.org"
 * - is_channel: true
 * - is_user: false
 * Note: this is the special empty-string channel, used internal in biboumi
 * but has no meaning on IRC.
 *
 * foo!irc.example.org
 * - local: "foo"
 * - server: "irc.example.org"
 * - is_channel: false
 * - is_user: true
 * Note: the empty-string user (!irc.example.org) has no special meaning in biboumi
 *
 * irc.example.org:
 * - local: ""
 * - server: "irc.example.org"
 * - is_channel: false
 * - is_user: false
 */
class Iid
{
public:
  Iid(const std::string& iid);
  Iid();
  Iid(const Iid&) = default;

  Iid(Iid&&) = delete;
  Iid& operator=(const Iid&) = delete;
  Iid& operator=(Iid&&) = delete;

  void set_local(const std::string& loc);
  void set_server(const std::string& serv);
  const std::string& get_local() const;
  const std::string& get_server() const;

  bool is_channel;
  bool is_user;

  std::string get_sep() const;

  std::tuple<std::string, std::string> to_tuple() const;

private:

  void init(const std::string& iid);
  void init_with_fixed_server(const std::string& iid, const std::string& hostname);

  std::string local;
  std::string server;
};

namespace std {
  const std::string to_string(const Iid& iid);
}

#endif // IID_INCLUDED
