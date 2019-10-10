from scenarios import *

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    sequences.handshake(),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection(),
    expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "{nick_one}"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_two}' />"),
    expect_stanza("/message/body[text()='Mode #bar [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#bar%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']",
                  "/presence/muc_user:x/muc_user:status[@code='210']", # This status signals that the server forced our nick to NOT be the one we asked
                  ),
    expect_stanza("/message[@from='#bar%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
)

