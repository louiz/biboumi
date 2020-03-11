from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send a ping to ourself
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='first_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_ping']"),

    # Now join the same room, from the same bare JID, behind the same nick
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),

    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

    # And re-send a self ping
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='second_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='second_ping']"),
    ## And re-do exactly the same thing, just change the resource initiating the self ping
    send_stanza("<iq type='get' from='{jid_one}/{resource_two}' id='third_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_two}'][@id='third_ping']"),
)
