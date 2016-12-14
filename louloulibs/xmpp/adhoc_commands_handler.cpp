#include <xmpp/adhoc_commands_handler.hpp>
#include <xmpp/xmpp_component.hpp>

#include <utils/timed_events.hpp>
#include <logger/logger.hpp>
#include <config/config.hpp>
#include <xmpp/jid.hpp>

#include <iostream>

using namespace std::string_literals;

const std::map<const std::string, const AdhocCommand>& AdhocCommandsHandler::get_commands() const
{
  return this->commands;
}

void AdhocCommandsHandler::add_command(std::string name, AdhocCommand command)
{
  const auto found = this->commands.find(name);
  if (found != this->commands.end())
    throw std::runtime_error("Trying to add an ad-hoc command that already exist: "s + name);
  this->commands.emplace(std::make_pair(std::move(name), std::move(command)));
}

XmlNode AdhocCommandsHandler::handle_request(const std::string& executor_jid, const std::string& to, XmlNode command_node)
{
  std::string action = command_node.get_tag("action");
  if (action.empty())
    action = "execute";
  command_node.del_tag("action");

  Jid jid(executor_jid);

  const std::string node = command_node.get_tag("node");
  auto command_it = this->commands.find(node);
  if (command_it == this->commands.end())
    {
      XmlSubNode error(command_node, ADHOC_NS":error");
      error["type"] = "cancel";
      XmlSubNode condition(error, STANZA_NS":item-not-found");
    }
  else if (command_it->second.is_admin_only() &&
           Config::get("admin", "") != jid.local + "@" + jid.domain)
    {
      XmlSubNode error(command_node, ADHOC_NS":error");
      error["type"] = "cancel";
      XmlSubNode condition(error, STANZA_NS":forbidden");
    }
  else
    {
      std::string sessionid = command_node.get_tag("sessionid");
      if (sessionid.empty())
        {                       // create a new session, with a new id
          sessionid = XmppComponent::next_id();
          command_node["sessionid"] = sessionid;
          this->sessions.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(sessionid, executor_jid),
                                 std::forward_as_tuple(command_it->second, executor_jid, to));
          TimedEventsManager::instance().add_event(TimedEvent(std::chrono::steady_clock::now() + 3600s,
                                                              std::bind(&AdhocCommandsHandler::remove_session, this, sessionid, executor_jid),
                                                              "adhocsession"s + sessionid + executor_jid));
        }
      auto session_it = this->sessions.find(std::make_pair(sessionid, executor_jid));
      if (session_it == this->sessions.end())
        {
          XmlSubNode error(command_node, ADHOC_NS":error");
          error["type"] = "modify";
          XmlSubNode condition(error, STANZA_NS":bad-request");
        }
      else if (action == "execute" || action == "next" || action == "complete")
        {
          // execute the step
          AdhocSession& session = session_it->second;
          const AdhocStep& step = session.get_next_step();
          step(this->xmpp_component, session, command_node);
          if (session.remaining_steps() == 0 ||
              session.is_terminated())
            {
              this->sessions.erase(session_it);
              command_node["status"] = "completed";
              TimedEventsManager::instance().cancel("adhocsession"s + sessionid + executor_jid);
            }
          else
            {
              command_node["status"] = "executing";
              XmlSubNode actions(command_node, "actions");
              XmlSubNode next(actions, "next");
            }
        }
      else if (action == "cancel")
        {
          this->sessions.erase(session_it);
          command_node["status"] = "canceled";
          TimedEventsManager::instance().cancel("adhocsession"s + sessionid + executor_jid);
        }
      else                      // unsupported action
        {
          XmlSubNode error(command_node, ADHOC_NS":error");
          error["type"] = "modify";
          XmlSubNode condition(error, STANZA_NS":bad-request");
        }
    }
  return command_node;
}

void AdhocCommandsHandler::remove_session(const std::string& session_id, const std::string& initiator_jid)
{
  auto session_it = this->sessions.find(std::make_pair(session_id, initiator_jid));
  if (session_it != this->sessions.end())
    {
      this->sessions.erase(session_it);
      return ;
    }
  log_error("Tried to remove ad-hoc session for [", session_id, ", ", initiator_jid, "] but none found");
}
