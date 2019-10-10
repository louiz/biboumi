from scenarios import *

def expect_self_join_presence(jid, chan, nick):
    return (
    expect_stanza("/message/body[text()='Mode " + chan + " [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='" + jid +"'][@from='" + chan + "%{irc_server_one}/" + nick + "']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='100']", # Rooms are all non-anonymous
                  "/presence/muc_user:x/muc_user:status[@code='110']",
                  ),
    expect_stanza("/message[@from='" + chan + "%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
    )


scenario = (
    sequences.handshake(),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection(),

    expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "{nick_one}"),

)

