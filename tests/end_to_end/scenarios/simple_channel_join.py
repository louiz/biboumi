from scenarios import *

def expect_self_join_presence(jid='{jid_one}/{resource_one}', chan='#foo', nick='{nick_one}', irc_server="{irc_server_one}"):
    return (
    expect_stanza("/presence[@to='" + jid +"'][@from='" + chan + "%" + irc_server + "/" + nick + "']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='100']", # Rooms are all non-anonymous
                  "/presence/muc_user:x/muc_user:status[@code='110']",
                  ),
    expect_stanza("/message[@from='" + chan + "%" + irc_server + "'][@type='groupchat']/subject[not(text())]"),
    )


scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection(),

    expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "{nick_one}"),

)

