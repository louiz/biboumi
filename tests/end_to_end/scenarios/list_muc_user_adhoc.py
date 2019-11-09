from scenarios import *

scenario = (
    send_stanza("<iq type='get' id='idwhatever' from='{jid_admin}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
    expect_stanza("/iq[@type='error']/error[@type='cancel']/stanza:feature-not-implemented"),
)
