#ifndef ADHOC_COMMAND_HPP
# define ADHOC_COMMAND_HPP

/**
 * Describe an ad-hoc command.
 *
 * Can only have zero or one step for now. When execution is requested, it
 * can return a result immediately, or provide a form to be filled, and
 * provide a result once the filled form is received.
 */

#include <xmpp/adhoc_session.hpp>

#include <functional>
#include <string>

class AdhocCommand
{
  friend class AdhocSession;
public:
  AdhocCommand(std::vector<AdhocStep>&& callback, const std::string& name, const bool admin_only);
  ~AdhocCommand();

  const std::string name;

  bool is_admin_only() const;

private:
  /**
   * A command may have one or more steps. Each step is a different
   * callback, inserting things into a <command/> XmlNode and calling
   * methods of an AdhocSession.
   */
  std::vector<AdhocStep> callbacks;
  const bool admin_only;
};

void PingStep1(XmppComponent*, AdhocSession& session, XmlNode& command_node);
void HelloStep1(XmppComponent*, AdhocSession& session, XmlNode& command_node);
void HelloStep2(XmppComponent*, AdhocSession& session, XmlNode& command_node);
void Reload(XmppComponent*, AdhocSession& session, XmlNode& command_node);

#endif // ADHOC_COMMAND_HPP
