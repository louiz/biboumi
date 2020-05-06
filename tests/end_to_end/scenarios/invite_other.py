from scenarios import *

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_two}/{resource_two}' to='#bar%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_two}'),
    expect_stanza("/presence"),
    expect_stanza("/message"),
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><x xmlns='http://jabber.org/protocol/muc#user'><invite to='{nick_two}'/></x></message>"),
    expect_stanza("/message/body[text()='{nick_two} has been invited to #foo']"),
    expect_stanza("/message[@to='{jid_two}/{resource_two}'][@from='#foo%{irc_server_one}']/muc_user:x/muc_user:invite[@from='#foo%{irc_server_one}/{nick_one}']"),

    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><x xmlns='http://jabber.org/protocol/muc#user'><invite to='bertrand@example.com'/></x></message>"),
    expect_stanza("/message[@to='bertrand@example.com'][@from='#foo%{irc_server_one}']/muc_user:x/muc_user:invite[@from='{jid_one}/{resource_one}']"),
)
