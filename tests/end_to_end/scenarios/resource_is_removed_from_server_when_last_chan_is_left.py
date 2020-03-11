from scenarios import *

scenario = (
    # Join the channel
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_one}']/subject[not(text())]"),

    # Make it persistent
    send_stanza("<iq from='{jid_one}/{resource_one}' id='conf1' to='#foo%{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/muc#owner'/></iq>"),
    expect_stanza("/iq[@type='result']/muc_owner:query/dataform:x/dataform:field[@var='persistent'][@type='boolean']/dataform:value[text()='false']"),
    send_stanza("<iq from='{jid_one}/{resource_one}' id='conf2' to='#foo%{irc_server_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#owner'><x type='submit' xmlns='jabber:x:data'><field var='persistent' xmlns='jabber:x:data'><value>true</value></field></x></query></iq>"),
    expect_stanza("/iq[@type='result']"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable'][@from='#foo%{irc_server_one}/{nick_one}']"),

    # Join the same channel, with the same JID, but a different resource
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

    # Join some other channel with someone else
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#bar%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
    expect_stanza("/message/body[text()='Mode #bar [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_two}/{resource_one}'][@from='#bar%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#bar%{irc_server_one}'][@type='groupchat'][@to='{jid_two}/{resource_one}']/subject[not(text())]"),

    # Send two messages from the second user to the first one
    send_stanza("<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>kikoo</body></message>"),
    send_stanza("<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>second kikoo</body></message>"),

    # We must receive each message only once, no duplicate
    expect_stanza("/message/body[text()='kikoo']"),
    expect_stanza("/message/body[text()='second kikoo']"),
)
