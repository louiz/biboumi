from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send a ping to ourself
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='first_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_ping']"),

    # Send a ping to ourself
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='first_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_ping']"),
)
