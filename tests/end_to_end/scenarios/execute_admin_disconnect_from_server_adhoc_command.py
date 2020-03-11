from scenarios import *

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    # Admin connects to first server
    send_stanza("<presence from='{jid_admin}/{resource_one}' to='#bar%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_admin}/{resource_one}'),
    expect_self_join_presence(jid = '{jid_admin}/{resource_one}', chan = "#bar", nick = "{nick_one}"),

    # Non-Admin connects to first server
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "{nick_two}"),

    # Non-admin connects to second server
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#bon%{irc_server_two}/{nick_three}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("{irc_host_two}", '{jid_one}/{resource_two}'),
    expect_self_join_presence(jid = '{jid_one}/{resource_two}', chan = "#bon", nick = "{nick_three}", irc_server = "{irc_server_two}"),

    # Execute as admin
    send_stanza("<iq type='set' id='command1' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-from-irc-server' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='disconnect-from-irc-server'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='jid'][@type='list-single']/dataform:option[@label='{jid_one}']/dataform:value[text()='{jid_one}']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='jid'][@type='list-single']/dataform:option[@label='{jid_admin}']/dataform:value[text()='{jid_admin}']",
                  "/iq/commands:command/commands:actions/commands:next",
                  after = save_value("sessionid", extract_attribute("/iq/commands:command[@node='disconnect-from-irc-server']", "sessionid"))
                 ),
    send_stanza("<iq type='set' id='command2' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-from-irc-server' sessionid='{sessionid}' action='next'><x xmlns='jabber:x:data' type='submit'><field var='jid'><value>{jid_one}</value></field><field var='quit-message'><value>e2e test one</value></field></x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='disconnect-from-irc-server'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='quit-message'][@type='text-single']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='irc-servers'][@type='list-multi']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='irc-servers']/dataform:option[@label='localhost']/dataform:value[text()='localhost']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='irc-servers']/dataform:option[@label='irc.localhost']/dataform:value[text()='irc.localhost']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  after = save_value("sessionid", extract_attribute("/iq/commands:command[@node='disconnect-from-irc-server']", "sessionid"))
                 ),
    # Command is successfull
    send_stanza("<iq type='set' id='command3' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-from-irc-server' sessionid='{sessionid}' action='complete'><x xmlns='jabber:x:data' type='submit'><field var='irc-servers'><value>localhost</value></field><field var='quit-message'><value>Disconnected by e2e</value></field></x></command></iq>"),
    # User is being disconnected
    expect_unordered(
             [
                 "/presence[@type='unavailable'][@to='{jid_one}/{resource_two}'][@from='#bon%{irc_server_two}/{nick_three}']",
                 "/presence/status[text()='Disconnected by e2e']"
             ],
             [
                 "/iq[@type='result']/commands:command[@node='disconnect-from-irc-server'][@status='completed']/commands:note[@type='info'][text()='{jid_one} was disconnected from 1 IRC server.']",
             ]),

    # Execute as non-admin (this skips the first step)
    send_stanza("<iq type='set' id='command4' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-from-irc-server' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='disconnect-from-irc-server'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='quit-message'][@type='text-single']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='irc-servers'][@type='list-multi']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='irc-servers']/dataform:option[@label='irc.localhost']/dataform:value[text()='irc.localhost']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  after = save_value("sessionid", extract_attribute("/iq/commands:command[@node='disconnect-from-irc-server']", "sessionid"))
                 ),
    send_stanza("<iq type='set' id='command5' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-from-irc-server' sessionid='{sessionid}' action='complete'><x xmlns='jabber:x:data' type='submit'><field var='irc-servers'><value>irc.localhost</value></field><field var='quit-message'><value>Disconnected by e2e</value></field></x></command></iq>"),
    expect_unordered(
             [
                 "/presence[@type='unavailable'][@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']",
                 "/presence/status[text()='Disconnected by e2e']"
             ],
             [
                 "/iq[@type='result']/commands:command[@node='disconnect-from-irc-server'][@status='completed']/commands:note[@type='info'][text()='{jid_one}/{resource_one} was disconnected from 1 IRC server.']",
             ]),
)
