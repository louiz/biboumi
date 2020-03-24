from scenarios import *

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#true\\2ffalse%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#true\\2ffalse%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#true\\2ffalse%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
)
