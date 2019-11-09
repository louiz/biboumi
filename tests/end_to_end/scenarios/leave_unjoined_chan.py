from scenarios import *

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/message"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_two}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection_begin("irc.localhost", '{jid_two}/{resource_two}'),

    expect_stanza("/message[@to='{jid_two}/{resource_two}'][@type='chat']/body[text()='irc.localhost: {nick_one}: Nickname is already in use.']"),
    expect_stanza("/presence[@type='error']/error[@type='cancel'][@code='409']/stanza:conflict"),
    send_stanza("<presence from='{jid_two}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' type='unavailable' />")
)
