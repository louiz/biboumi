from scenarios import *

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    sequences.handshake(),
    send_stanza("<presence from='{jid_admin}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_admin}/{resource_one}'),
    expect_self_join_presence(jid = '{jid_admin}/{resource_one}', chan = "#foo", nick = "{nick_one}"),

    send_stanza("<iq type='set' id='command1' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-user' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='disconnect-user'][@sessionid][@status='executing']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  after = save_value("sessionid", extract_attribute("/iq/commands:command[@node='disconnect-user']", "sessionid"))
                 ),
    send_stanza("<iq type='set' id='command2' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-user' sessionid='{sessionid}' action='complete'><x xmlns='jabber:x:data' type='submit'><field var='jids'><value>{jid_admin}</value></field><field var='quit-message'><value>Disconnected by e2e</value></field></x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='disconnect-user'][@status='completed']/commands:note[@type='info'][text()='1 user has been disconnected.']"),
    # Note, charybdis ignores our QUIT message, so we can't test it
    expect_stanza("/presence[@type='unavailable'][@to='{jid_admin}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']"),
)
