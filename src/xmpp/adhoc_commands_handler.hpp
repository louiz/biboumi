#ifndef ADHOC_COMMANDS_HANDLER_HPP
# define ADHOC_COMMANDS_HANDLER_HPP

/**
 * Manage a list of available AdhocCommands and the list of ongoing
 * AdhocCommandSessions.
 */

#include <xmpp/adhoc_command.hpp>
#include <xmpp/xmpp_stanza.hpp>

#include <utility>
#include <string>
#include <map>

class XmppComponent;

class AdhocCommandsHandler
{
public:
  explicit AdhocCommandsHandler(XmppComponent* xmpp_component);
  ~AdhocCommandsHandler();
  /**
   * Returns the list of available commands.
   */
  const std::map<const std::string, const AdhocCommand>& get_commands() const;
  /**
   * Find the requested command, create a new session or use an existing
   * one, and process the request (provide a new form, an error, or a
   * result).
   *
   * Returns a (moved) XmlNode that will be inserted in the iq response. It
   * should be a <command/> node containing one or more useful children. If
   * it contains an <error/> node, the iq response will have an error type.
   *
   * Takes a copy of the <command/> node so we can actually edit it and use
   * it as our return value.
   */
  XmlNode handle_request(const std::string& executor_jid, XmlNode command_node);
  /**
   * Remove the session from the list. This is done to avoid filling the
   * memory with waiting session (for example due to a client that starts
   * multi-steps command but never finishes them).
   */
  void remove_session(const std::string& session_id, const std::string& initiator_jid);
private:
  /**
   * A pointer to the XmppComponent, to access to basically anything in the
   * gateway.
   */
  XmppComponent* xmpp_component;
  /**
   * The list of all available commands.
   */
  const std::map<const std::string, const AdhocCommand> commands;
  /**
   * The list of all currently on-going commands.
   *
   * Of the form: {{session_id, owner_jid}, session}.
   */
  std::map<std::pair<const std::string, const std::string>, AdhocSession> sessions;

  AdhocCommandsHandler(const AdhocCommandsHandler&) = delete;
  AdhocCommandsHandler(AdhocCommandsHandler&&) = delete;
  AdhocCommandsHandler& operator=(const AdhocCommandsHandler&) = delete;
  AdhocCommandsHandler& operator=(AdhocCommandsHandler&&) = delete;
};

#endif // ADHOC_COMMANDS_HANDLER_HPP
