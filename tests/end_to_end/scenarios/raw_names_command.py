from functions import send_stanza, expect_stanza

import scenarios.simple_channel_join

join_channel = scenarios.simple_channel_join.scenario

scenario = (
    join_channel,

    send_stanza("<message type='chat' from='{jid_one}/{resource_one}' to='{irc_server_one}'><body>NAMES</body></message>"),
    expect_stanza("/message/body[text()='irc.localhost: = #foo @{nick_one} ']"),
    expect_stanza("/message/body[text()='irc.localhost: * End of /NAMES list. ']"),
)
