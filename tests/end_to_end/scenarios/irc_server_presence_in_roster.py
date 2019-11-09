from scenarios import *

scenario = (
    # Mutual subscription exchange
    send_stanza("<presence from='{jid_one}' to='{irc_server_one}' type='subscribe' id='subid1' />"),
    expect_stanza("/presence[@type='subscribed'][@id='subid1']"),

    expect_stanza("/presence[@type='subscribe']"),
    send_stanza("<presence from='{jid_one}' to='{irc_server_one}' type='subscribed' />"),

    # Join a channel on that server
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),

    # We must receive the IRC server presence, in the connection sequence
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}', expected_irc_presence=True),
    expect_stanza("/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

    # Leave the channel, and thus the IRC server
    send_stanza("<presence type='unavailable' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    expect_stanza("/presence[@type='unavailable'][@from='#foo%{irc_server_one}/{nick_one}']"),
    expect_stanza("/presence[@from='{irc_server_one}'][@to='{jid_one}'][@type='unavailable']"),
)
