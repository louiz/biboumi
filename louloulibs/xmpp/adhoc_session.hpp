#ifndef ADHOC_SESSION_HPP
# define ADHOC_SESSION_HPP

#include <xmpp/xmpp_stanza.hpp>

#include <functional>
#include <string>

class XmppComponent;

class AdhocCommand;
class AdhocSession;

/**
 * A function executed as an ad-hoc command step. It takes a <command/>
 * XmlNode and modifies it accordingly (inserting for example an <error/>
 * node, or a data form…).
 * TODO fix this:
 * It also must call one of step_passed(), cancel() etc on the AdhocSession object.
 */
typedef std::function<void(XmppComponent*, AdhocSession&, XmlNode&)> AdhocStep;

class AdhocSession
{
public:
  explicit AdhocSession(const AdhocCommand& command, const std::string& jid);
  ~AdhocSession();
  /**
   * Return the function to be executed, found in our AdhocCommand, for the
   * current_step. And increment the current_step.
   */
  const AdhocStep& get_next_step();
  /**
   * Return the number of remaining steps.
   */
  size_t remaining_steps() const;
  /**
   * This may be modified by an AdhocStep, to indicate that this session
   * should no longer exist, because we encountered an error, and we can't
   * execute any more step of it.
   */
  void terminate();
  bool is_terminated() const;

private:
  /**
   * A reference of the command concerned by this session. Used for example
   * to get the next step of that command, things like that.
   */
  const AdhocCommand& command;
  /**
   * The full JID of the XMPP user that created this session by executing
   * the first step of a command. Only that JID must be allowed to access
   * this session.
   */
  const std::string& owner_jid;
  /**
   * The current step we are at. It starts at zero. It is used to index the
   * associated AdhocCommand::callbacks vector.
   */
  size_t current_step;
  bool terminated;

  AdhocSession(const AdhocSession&) = delete;
  AdhocSession(AdhocSession&&) = delete;
  AdhocSession& operator=(const AdhocSession&) = delete;
  AdhocSession& operator=(AdhocSession&&) = delete;
};

#endif // ADHOC_SESSION_HPP
