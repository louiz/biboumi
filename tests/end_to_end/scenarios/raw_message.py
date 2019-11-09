from scenarios import *

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/message"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<message from='{jid_one}/{resource_one}' to='{irc_server_one}' type='chat'><body>WHOIS {nick_one}</body></message>"),
    expect_stanza("/message[@from='{irc_server_one}'][@type='chat']/body[text()='irc.localhost: {nick_one} ~{nick_one} localhost * {nick_one}']"),
)
