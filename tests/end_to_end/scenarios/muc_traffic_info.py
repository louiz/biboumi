from scenarios import *

scenario = (
    sequences.handshake(),

    send_stanza("<iq from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' id='1' type='get'><query xmlns='http://jabber.org/protocol/disco#info' node='http://jabber.org/protocol/muc#traffic'/></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='result']/disco_info:query[@node='http://jabber.org/protocol/muc#traffic']"),
)
