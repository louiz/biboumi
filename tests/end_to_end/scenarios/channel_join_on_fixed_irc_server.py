from scenarios import *

conf = "fixed_server"

scenario = (
    sequences.handshake(),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#zgeg@{biboumi_host}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}', fixed_irc_server=True),
    expect_stanza("/message/body[text()='Mode #zgeg [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#zgeg@{biboumi_host}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#zgeg@{biboumi_host}'][@type='groupchat']/subject[not(text())]"),
)
