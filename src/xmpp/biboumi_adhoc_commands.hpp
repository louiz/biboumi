#pragma once


#include <xmpp/adhoc_command.hpp>
#include <xmpp/adhoc_session.hpp>
#include <xmpp/xmpp_stanza.hpp>

class XmppComponent;

void DisconnectUserStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void DisconnectUserStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);

void ConfigureIrcServerStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void ConfigureIrcServerStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);

void ConfigureIrcChannelStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void ConfigureIrcChannelStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);

void DisconnectUserFromServerStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void DisconnectUserFromServerStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void DisconnectUserFromServerStep3(XmppComponent&, AdhocSession& session, XmlNode& command_node);


