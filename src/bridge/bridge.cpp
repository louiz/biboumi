#include <bridge/bridge.hpp>
#include <utility>
#include <xmpp/biboumi_component.hpp>
#include <network/poller.hpp>
#include <utils/empty_if_fixed_server.hpp>
#include <utils/encoding.hpp>
#include <utils/tolower.hpp>
#include <utils/uuid.hpp>
#include <logger/logger.hpp>
#include <utils/revstr.hpp>
#include <utils/split.hpp>
#include <xmpp/jid.hpp>
#include <database/database.hpp>
#include "result_set_management.hpp"
#include <algorithm>

using namespace std::string_literals;

static const char* action_prefix = "\01ACTION ";


static std::string in_encoding_for(const Bridge& bridge, const Iid& iid)
{
#ifdef USE_DATABASE
  const auto jid = bridge.get_bare_jid();
  return Database::get_encoding_in(jid, iid.get_server(), iid.get_local());
#else
  (void)bridge;
  (void)iid;
  return {"ISO-8859-1"};
#endif
}

Bridge::Bridge(std::string user_jid, BiboumiComponent& xmpp, std::shared_ptr<Poller>& poller):
 user_jid(std::move(user_jid)),
  xmpp(xmpp),
  poller(poller)
{
#ifdef USE_DATABASE
  const auto options = Database::get_global_options(this->user_jid);
  this->set_record_history(options.col<Database::RecordHistory>());
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
  for (auto& pair: this->irc_clients)
  {
    std::unique_ptr<IrcClient>& irc = pair.second;
    irc->send_quit_command(exit_message);
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
  return jid.bare();
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
                                std::make_unique<IrcClient>(this->poller, hostname,
                                                            nickname, username,
                                                            realname, jid.domain,
                                                            *this));
      std::unique_ptr<IrcClient>& irc = this->irc_clients.at(hostname);
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

bool Bridge::join_irc_channel(const Iid& iid, std::string nickname,
                              const std::string& password,
                              const std::string& resource,
                              HistoryLimit history_limit,
                              const bool force_join)
{
  const auto& hostname = iid.get_server();
#ifdef USE_DATABASE
  auto soptions = Database::get_irc_server_options(this->get_bare_jid(), hostname);
  if (!soptions.col<Database::Nick>().empty())
    nickname = soptions.col<Database::Nick>();
#endif
  IrcClient* irc = this->make_irc_client(hostname, nickname);
  irc->history_limit = history_limit;
  this->add_resource_to_server(hostname, resource);
  auto res_in_chan = this->is_resource_in_chan(ChannelKey{iid.get_local(), hostname}, resource);
  if (!res_in_chan)
    this->add_resource_to_chan(ChannelKey{iid.get_local(), hostname}, resource);
  if (irc->is_channel_joined(iid.get_local()) == false)
    {
      irc->send_join_command(iid.get_local(), password);
      return true;
    } else if (!res_in_chan || force_join) {
      // See https://github.com/xsf/xeps/pull/499 for the force_join argument
      this->generate_channel_join_for_resource(iid, resource);
    }
  return false;
}

void Bridge::send_channel_message(const Iid& iid, const std::string& body, std::string id)
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
  bool first = true;
  for (const std::string& line: lines)
    {
      std::string uuid;
#ifdef USE_DATABASE
      const auto xmpp_body = this->make_xmpp_body(line);
      if (this->record_history)
        uuid = Database::store_muc_message(this->get_bare_jid(), iid.get_local(), iid.get_server(), std::chrono::system_clock::now(),
                                    std::get<0>(xmpp_body), irc->get_own_nick());
#endif
      if (!first || id.empty())
        id = utils::gen_uuid();

      MessageCallback mirror_to_all_resources = [this, iid, uuid, id](const IrcClient* irc, const IrcMessage& message) {
        const std::string& line = message.arguments[1];
        for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
          this->xmpp.send_muc_message(std::to_string(iid), irc->get_own_nick(), this->make_xmpp_body(line),
                                      this->user_jid + "/" + resource, uuid, id);
      };

      if (line.substr(0, 5) == "/mode")
        {
          std::vector<std::string> args = utils::split(line.substr(5), ' ', false);
          irc->send_mode_command(iid.get_local(), args);
          continue;             // We do not want to send that back to the
                                // XMPP user, that’s not a textual message.
        }
      else if (line.substr(0, 4) == "/me ")
        irc->send_channel_message(iid.get_local(), action_prefix + line.substr(4) + "\01",
                                  std::move(mirror_to_all_resources));
      else
        irc->send_channel_message(iid.get_local(), line, std::move(mirror_to_all_resources));

      first = false;
    }
}

void Bridge::forward_affiliation_role_change(const Iid& iid, const std::string& from,
                                             const std::string& nick,
                                             const std::string& affiliation,
                                             const std::string& role,
                                             const std::string& id)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  IrcChannel* chan = irc->get_channel(iid.get_local());
  if (!chan || !chan->joined)
    return;
  IrcUser* user = chan->find_user(nick);
  if (!user)
    {
      this->xmpp.send_stanza_error("iq", from, std::to_string(iid), id, "cancel",
                                   "item-not-found", "no such nick", false);
      return;
    }
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

  irc_responder_callback_t cb = [this, iid, irc, id, from, nick](const std::string& irc_hostname, const IrcMessage& message) -> bool
  {
    if (irc_hostname != iid.get_server())
      return false;

    if (message.command == "MODE" && message.arguments.size() >= 2)
      {
        const std::string& chan_name = message.arguments[0];
        if (chan_name != iid.get_local())
          return false;
        const std::string actor_nick = IrcUser{message.prefix}.nick;
        if (!irc || irc->get_own_nick() != actor_nick)
          return false;

        this->xmpp.send_iq_result(id, from, std::to_string(iid));
      }
    else if (message.command == "401" && message.arguments.size() >= 2)
        {
          const std::string target_later = message.arguments[1];
          if (target_later != nick)
            return false;
          std::string error_message = "No such nick";
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp.send_stanza_error("iq", from, std::to_string(iid), id, "cancel", "item-not-found",
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
        this->xmpp.send_stanza_error("iq", from, std::to_string(iid), id, "cancel", "not-allowed",
                                     error_message, false);
      }
    else if (message.command == "472" && message.arguments.size() >= 2)
      {
          std::string error_message = "Unknown mode: " + message.arguments[1];
          if (message.arguments.size() >= 3)
            error_message = message.arguments[2];
          this->xmpp.send_stanza_error("iq", from, std::to_string(iid), id, "cancel", "not-allowed",
                                        error_message, false);
      }
    return true;
  };
  this->add_waiting_irc(std::move(cb));
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

  IrcChannel* channel = irc->get_channel(iid.get_local());

  const auto resources = this->number_of_resources_in_chan(key);
  if (resources == 1)
    {
      // Do not send a PART message if we actually are not in that channel
      // or if we already sent a PART but we are just waiting for the
      // acknowledgment from the server
      bool persistent = false;
#ifdef USE_DATABASE
      const auto goptions = Database::get_global_options(this->user_jid);
      if (goptions.col<Database::GlobalPersistent>())
        persistent = true;
      else
        {
          const auto coptions = Database::get_irc_channel_options_with_server_default(this->user_jid, iid.get_server(), iid.get_local());
          persistent = coptions.col<Database::Persistent>();
        }
#endif
      if (channel->joined && !channel->parting && !persistent)
        {
          irc->send_part_command(iid.get_local(), status_message);
        }
      else if (channel->joined)
        {
          this->send_muc_leave(iid, *channel->get_self(), "", true, true, resource, irc);
        }
      if (persistent)
        this->remove_resource_from_chan(key, resource);
      // Since there are no resources left in that channel, we don't
      // want to receive private messages using this room's JID
      this->remove_all_preferred_from_jid_of_room(iid.get_local());
    }
  else
    {
      if (channel && channel->joined)
        this->send_muc_leave(iid, *channel->get_self(),
                             "Biboumi note: " + std::to_string(resources - 1) + " resources are still in this channel.",
                             true, true, resource, irc);
      this->remove_resource_from_chan(key, resource);
    }
  if (this->number_of_channels_the_resource_is_in(iid.get_server(), resource) == 0)
    this->remove_resource_from_server(iid.get_server(), resource);
}

void Bridge::send_irc_nick_change(const Iid& iid, const std::string& new_nick, const std::string& requesting_resource)
{
  // We don’t change the nick if the presence was sent to a channel the resource is not in.
  auto res_in_chan = this->is_resource_in_chan(ChannelKey{iid.get_local(), iid.get_server()}, requesting_resource);
  if (!res_in_chan)
    return;
  IrcClient* irc = this->get_irc_client(iid.get_server());
  irc->send_nick_command(new_nick);
}

void Bridge::send_irc_channel_list_request(const Iid& iid, const std::string& iq_id, const std::string& to_jid,
                                           ResultSetInfo rs_info)
{
  auto& list = this->channel_list_cache[iid.get_server()];

  // We fetch the list from the IRC server only if we have a complete
  // cached list that needs to be invalidated (that is, when the request
  // doesn’t have a after or before, or when the list is empty).
  // If the list is not complete, this means that a request is already
  // ongoing, so we just need to add the callback.
  // By default the list is complete and empty.
  if (list.complete &&
      (list.channels.empty() || (rs_info.after.empty() && rs_info.before.empty())))
    {
      IrcClient* irc = this->get_irc_client(iid.get_server());
      irc->send_list_command();

      // Add a callback that will populate our list
      list.channels.clear();
      list.complete = false;
      irc_responder_callback_t cb = [this, iid](const std::string& irc_hostname,
                                                const IrcMessage& message) -> bool
      {
        if (irc_hostname != iid.get_server())
          return false;

        auto& list = this->channel_list_cache[iid.get_server()];

        if (message.command == "263" || message.command == "RPL_TRYAGAIN" || message.command == "ERR_TOOMANYMATCHES"
            || message.command == "ERR_NOSUCHSERVER")
          {
            list.complete = true;
            return true;
          }
        else if (message.command == "322" || message.command == "RPL_LIST")
          { // Add element to list
            if (message.arguments.size() == 4)
              {
                list.channels.emplace_back(message.arguments[1] + utils::empty_if_fixed_server("%" + iid.get_server()),
                                           message.arguments[2], message.arguments[3]);
              }
            return false;
          }
        else if (message.command == "323" || message.command == "RPL_LISTEND")
          { // Send the iq response with the content of the list
            list.complete = true;
            return true;
          }
        return false;
      };

      this->add_waiting_irc(std::move(cb));
    }

  // If the list is complete, we immediately send the answer.
  // Otherwise, we install a callback, that will populate our list and send
  // the answer when we can.
  if (list.complete)
    {
      this->send_matching_channel_list(list, rs_info, iq_id, to_jid, std::to_string(iid));
    }
  else
    {
      // Add a callback to answer the request as soon as we can
      irc_responder_callback_t cb = [this, iid, iq_id, to_jid,
                                     rs_info=std::move(rs_info)](const std::string& irc_hostname,
                                                                 const IrcMessage& message) -> bool
      {
        if (irc_hostname != iid.get_server())
          return false;

        if (message.command == "263" || message.command == "RPL_TRYAGAIN" || message.command == "ERR_TOOMANYMATCHES"
            || message.command == "ERR_NOSUCHSERVER")
          {
            std::string text;
            if (message.arguments.size() >= 2)
              text = message.arguments[1];
            this->xmpp.send_stanza_error("iq", to_jid, std::to_string(iid), iq_id, "wait", "service-unavailable", text, false);
            return true;
          }
        else if (message.command == "322" || message.command == "RPL_LIST")
          {
            auto& list = channel_list_cache[iid.get_server()];
            const auto res = this->send_matching_channel_list(list, rs_info, iq_id, to_jid, std::to_string(iid));
            return res;
          }
        else if (message.command == "323" || message.command == "RPL_LISTEND")
          { // Send the iq response with the content of the list
            auto& list = channel_list_cache[iid.get_server()];
            this->send_matching_channel_list(list, rs_info, iq_id, to_jid, std::to_string(iid));
            return true;
          }
        return false;
      };

      this->add_waiting_irc(std::move(cb));
    }
}

bool Bridge::send_matching_channel_list(const ChannelList& channel_list, const ResultSetInfo& rs_info,
                                        const std::string& id, const std::string& to_jid, const std::string& from)
{
  auto begin = channel_list.channels.begin();
  auto end = channel_list.channels.end();
  if (channel_list.complete)
    {
      begin = std::find_if(channel_list.channels.begin(), channel_list.channels.end(), [this, &rs_info](const ListElement& element)
      {
        return rs_info.after == element.channel + "@" + this->xmpp.get_served_hostname();
      });
      if (begin == channel_list.channels.end())
        begin = channel_list.channels.begin();
      else
        begin = std::next(begin);
      end = std::find_if(channel_list.channels.begin(), channel_list.channels.end(), [this, &rs_info](const ListElement& element)
      {
        return rs_info.before == element.channel + "@" + this->xmpp.get_served_hostname();
      });
      if (rs_info.max >= 0)
        {
          if (std::distance(begin, end) >= rs_info.max)
            end = begin + rs_info.max;
        }
    }
  else
    {
      if (rs_info.after.empty() && rs_info.before.empty() && rs_info.max < 0)
        return false;
      if (!rs_info.after.empty())
        {
          begin = std::find_if(channel_list.channels.begin(), channel_list.channels.end(), [this, &rs_info](const ListElement& element)
          {
            return rs_info.after == element.channel + "@" + this->xmpp.get_served_hostname();
          });
          if (begin == channel_list.channels.end())
            return false;
          begin = std::next(begin);
        }
        if (!rs_info.before.empty())
        {
          end = std::find_if(channel_list.channels.begin(), channel_list.channels.end(), [this, &rs_info](const ListElement& element)
          {
            return rs_info.before == element.channel + "@" + this->xmpp.get_served_hostname();
          });
          if (end == channel_list.channels.end())
            return false;
        }
      if (rs_info.max >= 0)
        {
          if (std::distance(begin, end) < rs_info.max)
            return false;
          else
            end = begin + rs_info.max;
        }
    }
  this->xmpp.send_iq_room_list_result(id, to_jid, from, channel_list, begin, end, rs_info);
  return true;
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

void Bridge::set_channel_topic(const Iid& iid, std::string subject)
{
  IrcClient* irc = this->get_irc_client(iid.get_server());
  std::string::size_type pos{0};
  while ((pos = subject.find('\n', pos)) != std::string::npos)
    subject[pos] = ' ';
  irc->send_topic_command(iid.get_local(), subject);
}

void Bridge::send_xmpp_version_to_irc(const Iid& iid, const std::string& name, const std::string& version, const std::string& os)
{
  std::string result(name + " " + version + " " + os);

  this->send_private_message(iid, "\01VERSION " + result + "\01", "NOTICE");
}

void Bridge::send_irc_ping_result(const Iid& iid, const std::string& id)
{
  this->send_private_message(iid, "\01PING " + utils::revstr(id) + "\01", "NOTICE");
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
  Jid from(to_jid);
  IrcClient* irc = this->get_irc_client(iid.get_server());
  IrcChannel* chan = irc->get_channel(iid.get_local());
  if (!chan->joined || !this->is_resource_in_chan(iid.to_tuple(), from.resource))
    {
      this->xmpp.send_stanza_error("iq", to_jid, from_jid, iq_id, "cancel", "not-acceptable",
                                    "", true);
      return;
    }
  if (chan->get_self()->nick == nick)
    {
      // XEP-0410 self-ping optimisation: always reply without going the full
      // round-trip through IRC and possibly another XMPP client. See the XEP
      // for details.
      Jid iq_from(from_jid);
      iq_from.local = std::to_string(iid);
      iq_from.resource = nick;

      Stanza iq("iq");
      iq["from"] = iq_from.full();
      iq["to"] = to_jid;
      iq["id"] = iq_id;
      iq["type"] = "result";
      this->xmpp.send_stanza(iq);
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
      if (message.command == "NOTICE" && utils::tolower(user.nick) == utils::tolower(target) &&
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

void Bridge::send_message(const Iid& iid, const std::string& nick, const std::string& body, const bool muc, const bool log)
{
  const auto encoding = in_encoding_for(*this, iid);
  std::string uuid{};
  if (muc)
    {
#ifdef USE_DATABASE
      const auto xmpp_body = this->make_xmpp_body(body, encoding);
      if (log && this->record_history)
        uuid = Database::store_muc_message(this->get_bare_jid(), iid.get_local(), iid.get_server(), std::chrono::system_clock::now(),
                                           std::get<0>(xmpp_body), nick);
#else
      (void)log;
#endif
      for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
        {
          this->xmpp.send_muc_message(std::to_string(iid), nick, this->make_xmpp_body(body, encoding),
                                      this->user_jid + "/" + resource, uuid, utils::gen_uuid());
        }
    }
  else
    {
      const auto it = this->preferred_user_from.find(iid.get_local());
      if (it != this->preferred_user_from.end())
        {
          const auto chan_name = Iid(Jid(it->second).local, {}).get_local();
          for (const auto& resource: this->resources_in_chan[ChannelKey{chan_name, iid.get_server()}])
            this->xmpp.send_message(it->second, this->make_xmpp_body(body, encoding),
                                    this->user_jid + "/"
                                    + resource, "chat", true, true, true);
        }
      else
        {
          for (const auto& resource: this->resources_in_server[iid.get_server()])
            this->xmpp.send_message(std::to_string(iid), this->make_xmpp_body(body, encoding),
                                    this->user_jid + "/" + resource, "chat", false, true);
        }
    }
}

void Bridge::send_presence_error(const Iid& iid, const std::string& nick,
                                 const std::string& type, const std::string& condition,
                                 const std::string& error_code, const std::string& text)
{
  this->xmpp.send_presence_error(std::to_string(iid), nick, this->user_jid, type, condition, error_code, text);
}

void Bridge::send_muc_leave(const Iid& iid, const IrcUser& user,
                            const std::string& message, const bool self,
                            const bool user_requested,
                            const std::string& resource,
                            const IrcClient* client)
{
  std::string affiliation;
  std::string role;
  std::tie(role, affiliation) = get_role_affiliation_from_irc_mode(user.get_most_significant_mode(client->get_sorted_user_modes()));

  if (!resource.empty())
    this->xmpp.send_muc_leave(std::to_string(iid), user.nick, this->make_xmpp_body(message),
                              this->user_jid + "/" + resource, self, user_requested, affiliation, role);
  else
    {
      for (const auto &res: this->resources_in_chan[iid.to_tuple()])
        this->xmpp.send_muc_leave(std::to_string(iid), user.nick, this->make_xmpp_body(message),
                                  this->user_jid + "/" + res, self, user_requested, affiliation, role);
      if (self)
        {
          // Copy the resources currently in that channel
          const auto resources_in_chan = this->resources_in_chan[iid.to_tuple()];

          this->remove_all_resources_from_chan(iid.to_tuple());

          // Now, for each resource that was in that channel, remove it from the server if it’s
          // not in any other channel
          for (const auto& r: resources_in_chan)
          if (this->number_of_channels_the_resource_is_in(iid.get_server(), r) == 0)
            this->remove_resource_from_server(iid.get_server(), r);
        }
    }
  IrcClient* irc = this->find_irc_client(iid.get_server());
  if (self && irc && irc->number_of_joined_channels() == 0)
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
      body = "\u000303" + user.nick + (user.host.empty()?
                                        "\u0003: ":
                                        (" (\u000310" + user.host + "\u000303)\u0003: ")) + msg;
    }
  else
    body = msg;

  const auto encoding = in_encoding_for(*this, {from, this});
  for (const auto& resource: this->resources_in_server[from])
    {
      if (Config::get("fixed_irc_server", "").empty())
        this->xmpp.send_message(from, this->make_xmpp_body(body, encoding), this->user_jid + "/" + resource, "chat", false, true);
      else
        this->xmpp.send_message("", this->make_xmpp_body(body, encoding), this->user_jid + "/" + resource, "chat", false, true);
    }
}

void Bridge::send_user_join(const std::string& hostname, const std::string& chan_name,
                            const IrcUser* user, const char user_mode, const bool self)
{
  const auto resources = this->resources_in_chan[ChannelKey{chan_name, hostname}];
  if (self && resources.empty())
    { // This was a forced join: no client ever asked to join this room,
      // but the server tells us we are in that room anyway.  XMPP can’t
      // do that, so we invite all the resources to join that channel.
      const Iid iid(chan_name, hostname, Iid::Type::Channel);
      this->send_xmpp_invitation(iid, "");
    }
  else
    {
      for (const auto& resource: resources)
        this->send_user_join(hostname, chan_name, user, user_mode, self, resource);
    }
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

  std::string encoded_nick_name(user->nick);
  xep0106::encode(encoded_nick_name);

  std::string full_jid =
      encoded_nick_name + utils::empty_if_fixed_server("%" + hostname)
      + "@" + this->xmpp.get_served_hostname()
      + "/" + user->host;

  this->xmpp.send_user_join(encoded_chan_name + utils::empty_if_fixed_server("%" + hostname),
                            user->nick, full_jid, affiliation, role,
                            this->user_jid + "/" + resource, self);
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

void Bridge::send_room_history(const std::string& hostname, const std::string& chan_name, const HistoryLimit& history_limit)
{
  for (const auto& resource: this->resources_in_chan[ChannelKey{chan_name, hostname}])
    this->send_room_history(hostname, chan_name, resource, history_limit);
}

void Bridge::send_room_history(const std::string& hostname, std::string chan_name, const std::string& resource, const HistoryLimit& history_limit)
{
#ifdef USE_DATABASE
  const auto goptions = Database::get_global_options(this->user_jid);
  auto limit = goptions.col<Database::MaxHistoryLength>();
  if (limit < 0)
    limit = 20;
  if (history_limit.stanzas >= 0 && history_limit.stanzas < limit)
    limit = history_limit.stanzas;
  const auto result = Database::get_muc_logs(this->user_jid, chan_name, hostname, static_cast<std::size_t>(limit), history_limit.since, {}, Id::unset_value, Database::Paging::last);
  const auto& lines = std::get<1>(result);
  chan_name.append(utils::empty_if_fixed_server("%" + hostname));
  for (const auto& line: lines)
    {
      const auto seconds = line.col<Database::Date>();
      this->xmpp.send_history_message(chan_name, line.col<Database::Nick>(), line.col<Database::Body>(),
                                      this->user_jid + "/" + resource, seconds);
    }
#else
  (void)hostname;
  (void)chan_name;
  (void)resource;
  (void)history_limit;
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

void Bridge::kick_muc_user(Iid&& iid, const std::string& target, const std::string& reason, const std::string& author,
                           const bool self)
{
  for (const auto& resource: this->resources_in_chan[iid.to_tuple()])
      this->xmpp.kick_user(std::to_string(iid), target, reason, author, this->user_jid + "/" + resource, self);
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
    this->xmpp.send_iq_version_request(utils::tolower(nick) + utils::empty_if_fixed_server("%" + hostname),
                                       this->user_jid + "/" + *resources.begin());
}

void Bridge::send_xmpp_ping_request(const std::string& nick, const std::string& hostname,
                                    const std::string& id)
{
  // Use revstr because the forwarded ping to target XMPP user must not be
  // the same as the request iq, but we also need to get it back easily
  // (revstr again)
  // Forward to the first resource (arbitrary, based on the “order” of the std::set) only
  const auto resources = this->resources_in_server[hostname];
  if (resources.begin() != resources.end())
    this->xmpp.send_ping_request(utils::tolower(nick) + utils::empty_if_fixed_server("%" + hostname),
                                 this->user_jid + "/" + *resources.begin(), utils::revstr(id));
}

void Bridge::send_xmpp_invitation(const Iid& iid, const std::string& author)
{
  for (const auto& resource: this->resources_in_server[iid.get_server()])
    this->xmpp.send_invitation(std::to_string(iid), this->user_jid + "/" + resource, author);
}

void Bridge::on_irc_client_connected(const std::string& hostname)
{
  this->xmpp.on_irc_client_connected(hostname, this->user_jid);
}

void Bridge::on_irc_client_disconnected(const std::string& hostname)
{
  this->xmpp.on_irc_client_disconnected(hostname, this->user_jid);
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

std::unordered_map<std::string, std::unique_ptr<IrcClient>>& Bridge::get_irc_clients()
{
  return this->irc_clients;
}

const std::unordered_map<std::string, std::unique_ptr<IrcClient>>& Bridge::get_irc_clients() const
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

void Bridge::remove_all_resources_from_chan(const Bridge::ChannelKey& channel)
{
  this->resources_in_chan.erase(channel);
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

std::size_t Bridge::number_of_resources_in_chan(const Bridge::ChannelKey& channel) const
{
  auto it = this->resources_in_chan.find(channel);
  if (it == this->resources_in_chan.end())
    return 0;
  return it->second.size();
}

std::size_t Bridge::number_of_channels_the_resource_is_in(const std::string& irc_hostname, const std::string& resource) const
{
  std::size_t res = 0;
  for (auto pair: this->resources_in_chan)
    {
      if (std::get<1>(pair.first) == irc_hostname && pair.second.count(resource) != 0)
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
  this->send_room_history(iid.get_server(), iid.get_local(), resource, irc->history_limit);
  this->send_topic(iid.get_server(), iid.get_encoded_local(), channel->topic, channel->topic_author, resource);
}

#ifdef USE_DATABASE
void Bridge::set_record_history(const bool val)
{
  this->record_history = val;
}
#endif
