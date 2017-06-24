#pragma once


#include <xmpp/adhoc_command.hpp>
#include <xmpp/adhoc_session.hpp>
#include <xmpp/xmpp_stanza.hpp>
#include <xmpp/jid.hpp>

class XmppComponent;

void DisconnectUserStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void DisconnectUserStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);

void ConfigureGlobalStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void ConfigureGlobalStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);

void ConfigureIrcServerStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void ConfigureIrcServerStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);

void ConfigureIrcChannelStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void insert_irc_channel_configuration_form(XmlNode& node, const Jid& requester, const Jid& target);
void ConfigureIrcChannelStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);
bool handle_irc_channel_configuration_form(const XmlNode& node, const Jid& requester, const Jid& target);

void DisconnectUserFromServerStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void DisconnectUserFromServerStep2(XmppComponent&, AdhocSession& session, XmlNode& command_node);
void DisconnectUserFromServerStep3(XmppComponent&, AdhocSession& session, XmlNode& command_node);

void GetIrcConnectionInfoStep1(XmppComponent&, AdhocSession& session, XmlNode& command_node);
