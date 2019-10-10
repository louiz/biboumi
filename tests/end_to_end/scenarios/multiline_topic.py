from scenarios import *

import scenarios.simple_channel_join

scenario = (
    scenarios.simple_channel_join.scenario,
    # User tries to set a multiline topic
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><subject>FIRST LINE\nSECOND LINE.</subject></message>"),
    # Server converts the newline into spaces, because IRC can’t have them in the topic
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat'][@to='{jid_one}/{resource_one}']/subject[text()='FIRST LINE SECOND LINE.']")
)
