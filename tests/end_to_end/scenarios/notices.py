from scenarios import *

import scenarios.simple_channel_join

scenario = (
    scenarios.simple_channel_join.scenario,

    send_stanza("<message from='{jid_one}/{resource_one}' to='{irc_server_one}' type='chat'><body>NOTICE {nick_one} :[#foo] Hello in a notice.</body></message>"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='[notice] [#foo] Hello in a notice.']"),
)
