#include <bridge/bridge.hpp>
#include <bridge/list_element.hpp>
#include <xmpp/biboumi_component.hpp>
#include <network/poller.hpp>
#include <utils/empty_if_fixed_server.hpp>
#include <utils/encoding.hpp>
#include <utils/tolower.hpp>
#include <logger/logger.hpp>
#include <utils/revstr.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <database/database.hpp>

using namespace std::string_literals;

static const char* action_prefix = "\01ACTION ";


static std::string in_encoding_for(const Bridge& bridge, const Iid& iid)
{
#ifdef USE_DATABASE
  const auto jid = bridge.get_bare_jid();
  auto options = Database::get_irc_channel_options_with_server_default(jid, iid.get_server(), iid.get_local());
  return options.encodingIn.value();
#else
  return {"ISO-8859-1"};
#endif
}

Bridge::Bridge(const std::string& user_jid, BiboumiComponent& xmpp, std::shared_ptr<Poller> poller):
  user_jid(user_jid),
  xmpp(xmpp),
  poller(poller)
{
#ifdef USE_DATABASE
  const auto options = Database::get_global_options(this->user_jid);
  this->set_record_history(options.recordHistory.value());
#endif
}

/**
 * Return the role and affiliation, corresponding to the given irc mode
 */
static std::tuple<std::string, std::string> get_role_affiliation_from_irc_mode(const char mode)
{
  if (mode == 'a' || mode == 'q'){
    return std::make_tuple("moderator", "owner");}
  else if (mode == 'o')
    return std::make_tuple("moderator", "admin");
  else if (mode == 'h')
    return std::make_tuple("moderator", "member");
  else if (mode == 'v')
    return std::make_tuple("participant", "member");
  else
    return std::make_tuple("participant", "none");
}

void Bridge::shutdown(const std::string& exit_message)
{
  for (auto it = this->irc_clients.begin(); it != this->irc_clients.end(); ++it)
  {
    it->second->send_quit_command(exit_message);
    it->second->leave_dummy_channel(exit_message);
  }
}

void Bridge::remove_resource(const std::string& resource,
                             const std::string& part_message)
{
  const auto resources_in_chan_copy = this->resources_in_chan;
  for (const auto& chan_pair: resources_in_chan_copy)
  {
    const ChannelKey& channel_key = chan_pair.first;
    const std::set<Resource>& resources = chan_pair.second;
    if (resources.count(resource))
      this->leave_irc_channel({std::get<0>(channel_key), std::get<1>(channel_key), {}},
                              part_message, resource);
  }
}

void Bridge::clean()
{
  auto it = this->irc_clients.begin();
  while (it != this->irc_clients.end())
  {
    IrcClient* client = it->second.get();
    if (!client->is_connected() && !client->is_connecting() &&
        !client->get_resolver().is_resolving())
      it = this->irc_clients.erase(it);
    else
      ++it;
  }
}

const std::string& Bridge::get_jid() const
{
  return this->user_jid;
}

std::string Bridge::get_bare_jid() const
{
  Jid jid(this->user_jid);
  return jid.local + "@" + jid.domain;
}

Xmpp::body Bridge::make_xmpp_body(const std::string& str, const std::string& encoding)
{
  std::string res;
  if (utils::is_valid_utf8(str.c_str()))
    res = str;
  else
    res = utils::convert_to_utf8(str, encoding.data());
  return irc_format_to_xhtmlim(res);
}

IrcClient* Bridge::make_irc_client(const std::string& hostname, const std::string& nickname)
{
  try
    {
      return this->irc_clients.at(hostname).get();
    }
  catch (const std::out_of_range& exception)
    {
      auto username = nickname;
      auto realname = nickname;
      Jid jid(this->user_jid);
      if (Config::get("realname_from_jid", "false") == "true")
        {
          username = jid.local;
          realname = this->get_bare_jid();
        }
      this->irc_clients.emplace(hostname,
                                std::make_shared<IrcClient>(this->poller, hostname,
                                                            nickname, username,
                                                            realname, jid.domain,
                                                            *this));
      std::shared_ptr<IrcClient> irc = this->irc_clients.at(hostname);
      return irc.get();
    }
}

IrcClient* Bridge::get_irc_client(const std::string& hostname)
{
  try
    {
      return this->irc_clients.at(hostname).get();
    }
  catch (const std::out_of_range& exception)
    {
      throw IRCNotConnected(hostname);
    }
}

IrcClient* Bridge::find_irc_client(const std::string& hostname) const
{
  try
    {
      return this->irc_clients.at(hostname).get();
    }
  catch (const std::out_of_range& exception)
    {
      return nullptr;
    }
}

bool Bridge::join_irc_channel(const Iid& iid, const std::string& nickname, const std::string& password,
                              const std::string& resource)
{
  const auto hostname = iid.get_server();
  IrcClient* irc = this->make_irc_client(hostname, nickname);
  this->add_resource_to_server(hostname, resource);
  auto res_in_chan = this->is_resource_in_chan(ChannelKey{iid.get_local(), hostname}, resource);
  if (!res_in_chan)
    this->add_resource_to_chan(ChannelKey{iid.get_local(), hostname}, resource);
  if (iid.get_local().empty())
    { // Join the dummy channel
      if (irc->is_welcomed())
        {
          if (irc->get_dummy_channel().joined)
            return false;
          // Immediately simulate a message coming from the IRC server saying that we
          // joined the channel
          const IrcMessage join_message(irc->get_nick(), "JOIN", {""});
          irc->on_channel_join(join_message);
          const IrcMessage end_join_message(std::string(iid.get_server()), "366",
                                            {irc->get_nick(),
                                                "", "End of NAMES list"});
          irc->on_channel_completely_joined(end_join_message);
        }
      else
        {
          irc->get_dummy_channel().joining = true;
          irc->start();
        }
      return true;
    }
  if (irc->is_channel_joined(iid.get_local()) == false)
    {
      irc->send_join_command(iid.get_local(), password);
      return true;
    } else if (!res_in_chan) {
      this->generate_channel_join_for_resource(iid, resource);
    }
  return false;
}

void Bridge::send_channel_message(const Iid& iid, const std::string& body)
{
  if (iid.get_server().empty())
    {
      for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
        this->xmpp.send_stanza_error("message", this->user_jid + "/" + resource, std::to_string(iid), "",
                                     "cancel", "remote-server-not-found",
                                     std::to_string(iid) + " is not a valid channel name. "
                                         "A correct room jid is of the form: #<chan>%<server>",
                                     false);
      return;
    }
  IrcClient* irc = this->get_irc_client(iid.get_server());

  // Because an IRC message cannot contain \n, we need to convert each line
  // of text into a separate IRC message. For conveniance, we also cut the
  // message into submessages on the XMPP side, this way the user of the
  // gateway sees what was actually sent over IRC.  For example if an user
  // sends “hello\n/me waves”, two messages will be generated: “hello” and
  // “/me waves”. Note that the “command” handling (messages starting with
  // /me, /mode, etc) is done for each message generated this way. In the
  // above example, the /me will be interpreted.
  std::vector<std::string> lines = utils::split(body, '\n', true);
  if (lines.empty())
    return ;
  for (const std::string& line: lines)
    {
      if (line.substr(0, 5) == "/mode")
        {
          std::vector<std::string> args = utils::split(line.substr(5), ' ', false);
          irc->send_mode_command(iid.get_local(), args);
          continue;             // We do not want to send that back to the
                                // XMPP user, that’s not a textual message.
        }
      else if (line.substr(0, 4) == "/me ")
        irc->send_channel_message(iid.get_local(), action_prefix + line.substr(4) + "\01");
      else
        irc->send_channel_message(iid.get_local(), line);

#ifdef USE_DATABASE
      const auto xmpp_body = this->make_xmpp_body(line);
      if (this->record_history)
        Database::store_muc_message(this->get_bare_jid(), iid, std::chrono::system_clock::now(),
                                    std::get<0>(xmpp_body), irc->get_own_nick());
#endif
      for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
        this->xmpp.send_muc_message(std::to_string(iid), irc->get_own_nick(),
                                    this->make_xmpp_body(line), this->user_jid + "/" + resource);
    }
}

void Bridge::forward_affiliation_role_change(const Iid& iid, const std::string& nick,
                                             const std::string& affiliation,
                                             const std::string& role)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  IrcChannel* chan = irc->get_channel(iid.get_local());
  if (!chan || !chan->joined)
    return;
  IrcUser* user = chan->find_user(nick);
  if (!user)
    return;
  // For each affiliation or role, we have a “maximal” mode that we want to
  // set. We must remove any superior mode at the same time. For example if
  // the user already has +o mode, and we set its affiliation to member, we
  // remove the +o mode, and add +v.  For each “superior” mode (for example,
  // for +v, the superior modes are 'h', 'a', 'o' and 'q') we check if that
  // user has it, and if yes we remove that mode

  std::size_t nb = 1;               // the number of times the nick must be
                                    // repeated in the argument list
  std::string modes;                // The string of modes to
                                    // add/remove. For example "+v-aoh"
  std::vector<char> modes_to_remove; // List of modes to check for removal
  if (affiliation == "none")
    {
      modes = "";
      nb = 0;
      modes_to_remove = {'v', 'h', 'o', 'a', 'q'};
    }
  else if (affiliation == "member")
    {
      modes = "+v";
      modes_to_remove = {'h', 'o', 'a', 'q'};
    }
  else if (role == "moderator")
    {
      modes = "+h";
      modes_to_remove = {'o', 'a', 'q'};
    }
  else if (affiliation == "admin")
    {
      modes = "+o";
      modes_to_remove = {'a', 'q'};
    }
  else if (affiliation == "owner")
    {
      modes = "+a";
      modes_to_remove = {'q'};
    }
  else
    return;
  for (const char mode: modes_to_remove)
    if (user->modes.find(mode) != user->modes.end())
      {
        modes += "-"s + mode;
        nb++;
      }
  if (modes.empty())
    return;
  std::vector<std::string> args(nb, nick);
  args.insert(args.begin(), modes);
  irc->send_mode_command(iid.get_local(), args);
}

void Bridge::send_private_message(const Iid& iid, const std::string& body, const std::string& type)
{
  if (iid.get_local().empty() || iid.get_server().empty())
    {
      this->xmpp.send_stanza_error("message", this->user_jid, std::to_string(iid), "",
                                    "cancel", "remote-server-not-found",
                                    std::to_string(iid) + " is not a valid channel name. "
                                    "A correct room jid is of the form: #<chan>%<server>",
                                    false);
      return;
    }
  IrcClient* irc = this->get_irc_client(iid.get_server());
  std::vector<std::string> lines = utils::split(body, '\n', true);
  if (lines.empty())
    return ;
  for (const std::string& line: lines)
    {
      if (line.substr(0, 4) == "/me ")
        irc->send_private_message(iid.get_local(), action_prefix + line.substr(4) + "\01", type);
      else
        irc->send_private_message(iid.get_local(), line, type);
    }
}

void Bridge::send_raw_message(const std::string& hostname, const std::string& body)
{
  IrcClient* irc = this->get_irc_client(hostname);
  irc->send_raw(body);
}

void Bridge::leave_irc_channel(Iid&& iid, const std::string& status_message, const std::string& resource)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  const auto key = iid.to_tuple();
  if (!this->is_resource_in_chan(key, resource))
    return ;

  const auto resources = this->number_of_resources_in_chan(key);
  if (resources == 1)
    {
      irc->send_part_command(iid.get_local(), status_message);
      // Since there are no resources left in that channel, we don't
      // want to receive private messages using this room's JID
      this->remove_all_preferred_from_jid_of_room(iid.get_local());
    }
  else
    {
      IrcChannel* chan = irc->get_channel(iid.get_local());
      if (chan)
        {
          auto nick = chan->get_self()->nick;
          this->remove_resource_from_chan(key, resource);
          this->send_muc_leave(std::move(iid), std::move(nick),
                               "Biboumi note: "s + std::to_string(resources - 1) + " resources are still in this channel.",
          true, resource);
          if (this->number_of_channels_the_resource_is_in(iid.get_server(), resource) == 0)
            this->remove_resource_from_server(iid.get_server(), resource);
        }
    }
}

void Bridge::send_irc_nick_change(const Iid& iid, const std::string& new_nick)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  irc->send_nick_command(new_nick);
}

void Bridge::send_irc_channel_list_request(const Iid& iid, const std::string& iq_id,
                                           const std::string& to_jid)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());

  irc->send_list_command();

  std::vector<ListElement> list;

  irc_responder_callback_t cb = [this, iid, iq_id, to_jid, list=std::move(list)](const std::string& irc_hostname,
                                                           const IrcMessage& message) mutable -> bool
    {
      if (irc_hostname != iid.get_server())
        return false;
      if (message.command == "263" || message.command == "RPL_TRYAGAIN" ||
          message.command == "ERR_TOOMANYMATCHES" || message.command == "ERR_NOSUCHSERVER")
        {
          std::string text;
          if (message.arguments.size() >= 2)
            text = message.arguments[1];
          this->xmpp.send_stanza_error("iq", to_jid, std::to_string(iid), iq_id,
                                        "wait", "service-unavailable", text, false);
          return true;
        }
      else if (message.command == "322" || message.command == "RPL_LIST")
        { // Add element to list
          if (message.arguments.size() == 4)
            list.emplace_back(message.arguments[1], message.arguments[2],
                              message.arguments[3]);
          return false;
        }
      else if (message.command == "323" || message.command == "RPL_LISTEND")
        { // Send the iq response with the content of the list
          this->xmpp.send_iq_room_list_result(iq_id, to_jid, std::to_string(iid), list);
          return true;
        }
      return false;
    };
  this->add_waiting_irc(std::move(cb));
}

void Bridge::send_irc_kick(const Iid& iid, const std::string& target, const std::string& reason,
                           const std::string& iq_id, const std::string& to_jid)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());

  irc->send_kick_command(iid.get_local(), target, reason);
  irc_responder_callback_t cb = [this, target, iq_id, to_jid, iid](const std::string& irc_hostname,
                                                                   const IrcMessage& message) -> bool
    {
      if (irc_hostname != iid.get_server())
        return false;
      if (message.command == "KICK" && message.arguments.size() >= 2)
        {
          const std::string target_later = message.arguments[1];
          const std::string chan_name_later = utils::tolower(message.arguments[0]);
          if (target_later != target || chan_name_later != iid.get_local())
            return false;
          this->xmpp.send_iq_result(iq_id, to_jid, std::to_string(iid));
        }
      else if (message.command == "401" && message.arguments.size() >= 2)
        {
          const std::string target_later = message.arguments[1];
          if (target_later != target)
            return false;
          std::string error_message = "No such nick";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp.send_stanza_error("iq", to_jid, std::to_string(iid), iq_id, "cancel", "item-not-found",
                                        error_message, false);
        }
      else if (message.command == "482" && message.arguments.size() >= 2)
        {
          const std::string chan_name_later = utils::tolower(message.arguments[1]);
          if (chan_name_later != iid.get_local())
            return false;
          std::string error_message = "You're not channel operator";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp.send_stanza_error("iq", to_jid, std::to_string(iid), iq_id, "cancel", "not-allowed",
                                        error_message, false);
        }
      return true;
    };
  this->add_waiting_irc(std::move(cb));
}

void Bridge::set_channel_topic(const Iid& iid, const std::string& subject)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  irc->send_topic_command(iid.get_local(), subject);
}

void Bridge::send_xmpp_version_to_irc(const Iid& iid, const std::string& name, const std::string& version, const std::string& os)
{
  std::string result(name + " " + version + " " + os);

  this->send_private_message(iid, "\01VERSION "s + result + "\01", "NOTICE");
}

void Bridge::send_irc_ping_result(const Iid& iid, const std::string& id)
{
  this->send_private_message(iid, "\01PING "s + utils::revstr(id) + "\01", "NOTICE");
}

void Bridge::send_irc_user_ping_request(const std::string& irc_hostname, const std::string& nick,
                                        const std::string& iq_id, const std::string& to_jid,
                                        const std::string& from_jid)
{
  Iid iid(nick, irc_hostname, Iid::Type::User);
  this->send_private_message(iid, "\01PING " + iq_id + "\01");

  irc_responder_callback_t cb = [this, nick=utils::tolower(nick), iq_id, to_jid, irc_hostname, from_jid]
          (const std::string& hostname, const IrcMessage& message) -> bool
    {
      if (irc_hostname != hostname || message.arguments.size() < 2)
        return false;
      IrcUser user(message.prefix);
      const std::string body = message.arguments[1];
      if (message.command == "NOTICE" && utils::tolower(user.nick) == nick
          && body.substr(0, 6) == "\01PING ")
        {
          const std::string id = body.substr(6, body.size() - 7);
          if (id != iq_id)
            return false;
          this->xmpp.send_iq_result_full_jid(iq_id, to_jid, from_jid);
          return true;
        }
      if (message.command == "401" && message.arguments[1] == nick)
        {
          std::string error_message = "No such nick";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp.send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "service-unavailable",
                                        error_message, true);
          return true;
        }

      return false;
    };
  this->add_waiting_irc(std::move(cb));
}

void Bridge::send_irc_participant_ping_request(const Iid& iid, const std::string& nick,
                                               const std::string& iq_id, const std::string& to_jid,
                                               const std::string& from_jid)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  IrcChannel* chan = irc->get_channel(iid.get_local());
  if (!chan->joined)
    {
      this->xmpp.send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "not-allowed",
                                    "", true);
      return;
    }
  if (chan->get_self()->nick != nick && !chan->find_user(nick))
    {
      this->xmpp.send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "item-not-found",
                                    "Recipient not in room", true);
      return;
    }

  // The user is in the room, send it a direct PING
  this->send_irc_user_ping_request(iid.get_server(), nick, iq_id, to_jid, from_jid);
}

void Bridge::on_gateway_ping(const std::string& irc_hostname, const std::string& iq_id, const std::string& to_jid,
                             const std::string& from_jid)
{
  Jid jid(from_jid);
  if (irc_hostname.empty() || this->find_irc_client(irc_hostname))
    this->xmpp.send_iq_result(iq_id, to_jid, jid.local);
  else
    this->xmpp.send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "service-unavailable",
                                  "", true);
}

void Bridge::send_irc_invitation(const Iid& iid, const std::string& to)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  Jid to_jid(to);
  std::string target_nick;
  // Many ways to address a nick:
  // A jid (ANY jid…) with a resource
  if (!to_jid.resource.empty())
    target_nick = to_jid.resource;
  else if (!to_jid.local.empty()) // A jid with a iid with a local part
    target_nick = Iid(to_jid.local, {}).get_local();
  else
    target_nick = to; // Not a jid, just the nick
  irc->send_invitation(iid.get_local(), target_nick);
}

void Bridge::send_irc_version_request(const std::string& irc_hostname, const std::string& target,
                                      const std::string& iq_id, const std::string& to_jid,
                                      const std::string& from_jid)
{
  Iid iid(target, irc_hostname, Iid::Type::User);
  this->send_private_message(iid, "\01VERSION\01");
  // TODO, add a timer to remove that waiting iq if the server does not
  // respond with a matching command before n seconds
  irc_responder_callback_t cb = [this, target, iq_id, to_jid, irc_hostname, from_jid]
          (const std::string& hostname, const IrcMessage& message) -> bool
    {
      if (irc_hostname != hostname)
        return false;
      IrcUser user(message.prefix);
      if (message.command == "NOTICE" && user.nick == target &&
          message.arguments.size() >= 2 && message.arguments[1].substr(0, 9) == "\01VERSION ")
        {
          // remove the "\01VERSION " and the "\01" parts from the string
          const std::string version = message.arguments[1].substr(9, message.arguments[1].size() - 10);
          this->xmpp.send_version(iq_id, to_jid, from_jid, version);
          return true;
        }
      if (message.command == "401" && message.arguments.size() >= 2
          && message.arguments[1] == target)
        {
          std::string error_message = "No such nick";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp.send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "item-not-found",
                                        error_message, true);
          return true;
        }
      return false;
    };
  this->add_waiting_irc(std::move(cb));
}

void Bridge::send_message(const Iid& iid, const std::string& nick, const std::string& body, const bool muc)
{
  const auto encoding = in_encoding_for(*this, iid);
  if (muc)
    {
#ifdef USE_DATABASE
      const auto xmpp_body = this->make_xmpp_body(body, encoding);
      if (!nick.empty() && this->record_history)
        Database::store_muc_message(this->get_bare_jid(), iid, std::chrono::system_clock::now(),
                                    std::get<0>(xmpp_body), nick);
#endif
      for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
        {
          this->xmpp.send_muc_message(std::to_string(iid), nick,
                                      this->make_xmpp_body(body, encoding), this->user_jid + "/" + resource);

        }
    }
  else
    {
      std::string target = std::to_string(iid);
      const auto it = this->preferred_user_from.find(iid.get_local());
      if (it != this->preferred_user_from.end())
        {
          const auto chan_name = Iid(Jid(it->second).local, {}).get_local();
          for (const auto& resource: this->resources_in_chan[ChannelKey{chan_name, iid.get_server()}])
            this->xmpp.send_message(it->second, this->make_xmpp_body(body, encoding),
                                    this->user_jid + "/" + resource, "chat", true);
        }
      else
        {
          for (const auto& resource: this->resources_in_server[iid.get_server()])
            this->xmpp.send_message(std::to_string(iid), this->make_xmpp_body(body, encoding),
                                    this->user_jid + "/" + resource, "chat", false);
        }
    }
}

void Bridge::send_presence_error(const Iid& iid, const std::string& nick,
                                 const std::string& type, const std::string& condition,
                                 const std::string& error_code, const std::string& text)
{
  this->xmpp.send_presence_error(std::to_string(iid), nick, this->user_jid, type, condition, error_code, text);
}

void Bridge::send_muc_leave(Iid&& iid, std::string&& nick, const std::string& message, const bool self,
                            const std::string& resource)
{
  if (!resource.empty())
    this->xmpp.send_muc_leave(std::to_string(iid), std::move(nick), this->make_xmpp_body(message),
                              this->user_jid + "/" + resource, self);
  else
    for (const auto& res: this->resources_in_chan[iid.to_tuple()])
      this->xmpp.send_muc_leave(std::to_string(iid), std::move(nick), this->make_xmpp_body(message),
                                this->user_jid + "/" + res, self);
  IrcClient* irc = this->find_irc_client(iid.get_server());
  if (irc && irc->number_of_joined_channels() == 0)
    irc->send_quit_command("");
}

void Bridge::send_nick_change(Iid&& iid,
                              const std::string& old_nick,
                              const std::string& new_nick,
                              const char user_mode,
                              const bool self)
{
  std::string affiliation;
  std::string role;
  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(user_mode);

  for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
    this->xmpp.send_nick_change(std::to_string(iid),
                                old_nick, new_nick, affiliation, role, this->user_jid + "/" + resource, self);
}

void Bridge::send_xmpp_message(const std::string& from, const std::string& author, const std::string& msg)
{
  std::string body;
  if (!author.empty())
    {
      IrcUser user(author);
      body = "\u000303"s + user.nick + (user.host.empty()?
                                        "\u0003: ":
                                        (" (\u000310" + user.host + "\u000303)\u0003: ")) + msg;
    }
  else
    body = msg;

  const auto encoding = in_encoding_for(*this, {from, this});
  for (const auto& resource: this->resources_in_server[from])
    {
        this->xmpp.send_message(from, this->make_xmpp_body(body, encoding), this->user_jid + "/" + resource, "chat");
    }
}

void Bridge::send_user_join(const std::string& hostname, const std::string& chan_name,
                            const IrcUser* user, const char user_mode, const bool self)
{
  for (const auto& resource: this->resources_in_chan[ChannelKey{chan_name, hostname}])
    this->send_user_join(hostname, chan_name, user, user_mode, self, resource);
}

void Bridge::send_user_join(const std::string& hostname, const std::string& chan_name,
                       const IrcUser* user, const char user_mode,
                       const bool self, const std::string& resource)
{
  std::string affiliation;
  std::string role;
  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(user_mode);

  std::string encoded_chan_name(chan_name);
  xep0106::encode(encoded_chan_name);

  this->xmpp.send_user_join(encoded_chan_name + utils::empty_if_fixed_server("%" + hostname), user->nick, user->host,
                            affiliation, role, this->user_jid + "/" + resource, self);
}

void Bridge::send_topic(const std::string& hostname, const std::string& chan_name, const std::string& topic,
                        const std::string& who)
{
  for (const auto& resource: this->resources_in_chan[ChannelKey{chan_name, hostname}])
    {
      this->send_topic(hostname, chan_name, topic, who, resource);
    }
}

void Bridge::send_topic(const std::string& hostname, const std::string& chan_name,
                        const std::string& topic, const std::string& who,
                        const std::string& resource)
{
  std::string encoded_chan_name(chan_name);
  xep0106::encode(encoded_chan_name);
  const auto encoding = in_encoding_for(*this, {encoded_chan_name, hostname, Iid::Type::Channel});
  this->xmpp.send_topic(encoded_chan_name + utils::empty_if_fixed_server(
      "%" + hostname), this->make_xmpp_body(topic, encoding), this->user_jid + "/" + resource, who);

}

void Bridge::send_room_history(const std::string& hostname, const std::string& chan_name)
{
  for (const auto& resource: this->resources_in_chan[ChannelKey{chan_name, hostname}])
    this->send_room_history(hostname, chan_name, resource);
}

void Bridge::send_room_history(const std::string& hostname, const std::string& chan_name, const std::string& resource)
{
#ifdef USE_DATABASE
  const auto coptions = Database::get_irc_channel_options_with_server_and_global_default(this->user_jid, hostname, chan_name);
  const auto lines = Database::get_muc_logs(this->user_jid, chan_name, hostname, coptions.maxHistoryLength.value());
  for (const auto& line: lines)
    {
      const auto seconds = line.date.value().timeStamp();
      this->xmpp.send_history_message(chan_name + utils::empty_if_fixed_server("%" + hostname), line.nick.value(),
                                      line.body.value(),
                                      this->user_jid + "/" + resource, seconds);
    }
#endif
}

std::string Bridge::get_own_nick(const Iid& iid)
{
  IrcClient* irc = this->find_irc_client(iid.get_server());
  if (irc)
    return irc->get_own_nick();
  return {};
}

size_t Bridge::active_clients() const
{
  return this->irc_clients.size();
}

void Bridge::kick_muc_user(Iid&& iid, const std::string& target, const std::string& reason, const std::string& author)
{
  for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
    this->xmpp.kick_user(std::to_string(iid), target, reason, author, this->user_jid + "/" + resource);
}

void Bridge::send_nickname_conflict_error(const Iid& iid, const std::string& nickname)
{
    for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
        this->xmpp.send_presence_error(std::to_string(iid), nickname, this->user_jid + "/" + resource,
                                       "cancel", "conflict", "409", "");
}

void Bridge::send_affiliation_role_change(const Iid& iid, const std::string& target, const char mode)
{
  std::string role;
  std::string affiliation;

  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(mode);
  for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
    this->xmpp.send_affiliation_role_change(std::to_string(iid), target, affiliation, role,
                                            this->user_jid + "/" + resource);
}

void Bridge::send_iq_version_request(const std::string& nick, const std::string& hostname)
{
  const auto resources = this->resources_in_server[hostname];
  if (resources.begin() != resources.end())
    this->xmpp.send_iq_version_request(utils::tolower(nick) + "%" + utils::empty_if_fixed_server(hostname),
                                       this->user_jid + "/" + *resources.begin());
}

void Bridge::send_xmpp_ping_request(const std::string& nick, const std::string& hostname,
                                    const std::string& id)
{
  // Use revstr because the forwarded ping to target XMPP user must not be
  // the same that the request iq, but we also need to get it back easily
  // (revstr again)
  // Forward to the first resource (arbitrary, based on the “order” of the std::set) only
  const auto resources = this->resources_in_server[hostname];
  if (resources.begin() != resources.end())
    this->xmpp.send_ping_request(utils::tolower(nick) + "%" + utils::empty_if_fixed_server(hostname),
                                 this->user_jid + "/" + *resources.begin(), utils::revstr(id));
}

void Bridge::send_xmpp_invitation(const Iid& iid, const std::string& author)
{
  for (const auto& resource: this->resources_in_server[iid.get_server()])
    this->xmpp.send_invitation(std::to_string(iid), this->user_jid + "/" + resource, author);
}

void Bridge::set_preferred_from_jid(const std::string& nick, const std::string& full_jid)
{
  auto it = this->preferred_user_from.find(nick);
  if (it == this->preferred_user_from.end())
    this->preferred_user_from.emplace(nick, full_jid);
  else
    this->preferred_user_from[nick] = full_jid;
}

void Bridge::remove_preferred_from_jid(const std::string& nick)
{
  auto it = this->preferred_user_from.find(nick);
  if (it != this->preferred_user_from.end())
    this->preferred_user_from.erase(it);
}

void Bridge::remove_all_preferred_from_jid_of_room(const std::string& channel_name)
{
  for (auto it = this->preferred_user_from.begin(); it != this->preferred_user_from.end();)
    {
      Iid iid(Jid(it->second).local, {});
      if (iid.get_local() == channel_name)
        it = this->preferred_user_from.erase(it);
      else
        ++it;
    }
}

void Bridge::add_waiting_irc(irc_responder_callback_t&& callback)
{
  this->waiting_irc.emplace_back(std::move(callback));
}

void Bridge::trigger_on_irc_message(const std::string& irc_hostname, const IrcMessage& message)
{
  auto it = this->waiting_irc.begin();
  while (it != this->waiting_irc.end())
    {
      if ((*it)(irc_hostname, message) == true)
        it = this->waiting_irc.erase(it);
      else
        ++it;
    }
}

std::unordered_map<std::string, std::shared_ptr<IrcClient>>& Bridge::get_irc_clients()
{
  return this->irc_clients;
}

std::set<char> Bridge::get_chantypes(const std::string& hostname) const
{
  IrcClient* irc = this->find_irc_client(hostname);
  if (!irc)
    return {'#', '&'};
  return irc->get_chantypes();
}

void Bridge::add_resource_to_chan(const Bridge::ChannelKey& channel, const std::string& resource)
{
  auto it = this->resources_in_chan.find(channel);
  if (it == this->resources_in_chan.end())
    this->resources_in_chan[channel] = {resource};
  else
    it->second.insert(resource);
}

void Bridge::remove_resource_from_chan(const Bridge::ChannelKey& channel, const std::string& resource)
{
  auto it = this->resources_in_chan.find(channel);
  if (it != this->resources_in_chan.end())
    {
      it->second.erase(resource);
      if (it->second.empty())
        this->resources_in_chan.erase(it);
    }
}

bool Bridge::is_resource_in_chan(const Bridge::ChannelKey& channel, const std::string& resource) const
{
  auto it = this->resources_in_chan.find(channel);
  if (it != this->resources_in_chan.end())
    if (it->second.count(resource) == 1)
      return true;
  return false;
}

void Bridge::add_resource_to_server(const Bridge::IrcHostname& irc_hostname, const std::string& resource)
{
  auto it = this->resources_in_server.find(irc_hostname);
  if (it == this->resources_in_server.end())
    this->resources_in_server[irc_hostname] = {resource};
  else
    it->second.insert(resource);
}

void Bridge::remove_resource_from_server(const Bridge::IrcHostname& irc_hostname, const std::string& resource)
{
  auto it = this->resources_in_server.find(irc_hostname);
  if (it != this->resources_in_server.end())
    {
      it->second.erase(resource);
      if (it->second.empty())
        this->resources_in_server.erase(it);
    }
}

bool Bridge::is_resource_in_server(const Bridge::IrcHostname& irc_hostname, const std::string& resource) const
{
  auto it = this->resources_in_server.find(irc_hostname);
  if (it != this->resources_in_server.end())
    if (it->second.count(resource) == 1)
      return true;
  return false;
}

std::size_t Bridge::number_of_resources_in_chan(const Bridge::ChannelKey& channel_key) const
{
  auto it = this->resources_in_chan.find(channel_key);
  if (it == this->resources_in_chan.end())
    return 0;
  return it->second.size();
}

std::size_t Bridge::number_of_channels_the_resource_is_in(const std::string& irc_hostname, const std::string& resource) const
{
  std::size_t res = 0;
  for (auto pair: this->resources_in_chan)
    {
      if (std::get<0>(pair.first) == irc_hostname && pair.second.count(resource) != 0)
        res++;
    }
  return res;
}

void Bridge::generate_channel_join_for_resource(const Iid& iid, const std::string& resource)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  IrcChannel* channel = irc->get_channel(iid.get_local());
  const auto self = channel->get_self();

  // Send the occupant list
  for (const auto& user: channel->get_users())
    {
      if (user->nick != self->nick)
        {
          this->send_user_join(iid.get_server(), iid.get_encoded_local(),
                               user.get(), user->get_most_significant_mode(irc->get_sorted_user_modes()),
                               false, resource);
        }
    }
  this->send_user_join(iid.get_server(), iid.get_encoded_local(),
                       self, self->get_most_significant_mode(irc->get_sorted_user_modes()),
                       true, resource);
  this->send_room_history(iid.get_server(), iid.get_local(), resource);
  this->send_topic(iid.get_server(), iid.get_encoded_local(), channel->topic, channel->topic_author, resource);
}

#ifdef USE_DATABASE
void Bridge::set_record_history(const bool val)
{
  this->record_history = val;
}
#endif