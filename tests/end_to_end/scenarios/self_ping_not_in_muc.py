from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send a ping to ourself, in a muc where we’re not
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='first_ping' to='#nil%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    # Immediately receive an error
    expect_stanza("/iq[@from='#nil%{irc_server_one}/{nick_one}'][@type='error'][@to='{jid_one}/{resource_one}'][@id='first_ping']/error/stanza:not-acceptable"),

    # Send a ping to ourself, in a muc where we are, but not this resource
    send_stanza("<iq type='get' from='{jid_one}/{resource_two}' id='first_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    # Immediately receive an error
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='error'][@to='{jid_one}/{resource_two}'][@id='first_ping']/error/stanza:not-acceptable"),
)
