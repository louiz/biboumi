from scenarios import *

scenario = (
    sequences.handshake(),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#true\\2ffalse%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/message/body[text()='Mode #true/false [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#true\\2ffalse%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#true\\2ffalse%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
)
