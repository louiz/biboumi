#ifndef BIBOUMI_ADHOC_COMMANDS_HPP_INCLUDED
#define BIBOUMI_ADHOC_COMMANDS_HPP_INCLUDED

#include <xmpp/adhoc_command.hpp>
#include <xmpp/adhoc_session.hpp>
#include <xmpp/xmpp_stanza.hpp>

class XmppComponent;

void DisconnectUserStep1(XmppComponent*, AdhocSession& session, XmlNode& command_node);
void DisconnectUserStep2(XmppComponent*, AdhocSession& session, XmlNode& command_node);

#endif /* BIBOUMI_ADHOC_COMMANDS_HPP_INCLUDED */
