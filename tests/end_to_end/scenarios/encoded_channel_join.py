from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#biboumi\\40louiz.org\\3a80%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/message/body[text()='Mode #biboumi@louiz.org:80 [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#biboumi\\40louiz.org\\3a80%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#biboumi\\40louiz.org\\3a80%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
)
