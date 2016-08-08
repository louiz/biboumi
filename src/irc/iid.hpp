#pragma once


#include <string>
#include <set>

class Bridge;

/**
 * A name representing an IRC channel on an IRC server, or an IRC user on an
 * IRC server, or just an IRC server.
 *
 * The separator is '%' between the local part (nickname or channel) and the
 * server part. If no separator is present, it's just an irc server.
 * If it is present, the first character of the local part determines if it’s
 * a channel or a user: ff the local part is empty or if its first character
 * is part of the chantypes characters, then it’s a channel, otherwise it’s
 * a user.
 *
 * It’s possible to have an empty-string server, but it makes no sense in
 * biboumi’s context.
 *
 * Assuming the chantypes are '#' and '&':
 *
 * #test%irc.example.org has :
 * - local: "#test" (the # is part of the name, it could very well be absent, or & (for example) instead)
 * - server: "irc.example.org"
 * - type: channel
 *
 * %irc.example.org:
 * - local: ""
 * - server: "irc.example.org"
 * - type: channel
 * Note: this is the special empty-string channel, used internally in biboumi
 * but has no meaning on IRC.
 *
 * foo%irc.example.org
 * - local: "foo"
 * - server: "irc.example.org"
 * - type: user
 * Note: the empty-string user (!irc.example.org) makes no sense for biboumi
 *
 * irc.example.org:
 * - local: ""
 * - server: "irc.example.org"
 * - type: server
 */
class Iid
{
public:
  enum class Type
  {
      Channel,
      User,
      Server,
  };
  static constexpr auto separator = "%";
  Iid(const std::string& iid, const std::set<char>& chantypes);
  Iid(const std::string& iid, const std::initializer_list<char>& chantypes);
  Iid(const std::string& iid, const Bridge* bridge);
  Iid(const std::string local, const std::string server, Type type);
  Iid() = default;
  Iid(const Iid&) = default;

  Iid(Iid&&) = delete;
  Iid& operator=(const Iid&) = delete;
  Iid& operator=(Iid&&) = delete;

  void set_local(const std::string& loc);
  void set_server(const std::string& serv);

  const std::string& get_local() const;
  const std::string get_encoded_local() const;
  const std::string& get_server() const;

  std::tuple<std::string, std::string> to_tuple() const;

  Type type { Type::Server };

private:

  void init(const std::string& iid);
  void set_type(const std::set<char>& chantypes);

  std::string local;
  std::string server;
};

namespace std {
  const std::string to_string(const Iid& iid);
}


