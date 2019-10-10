from functions import send_stanza, expect_stanza

import scenarios.simple_channel_join

join_channel = scenarios.simple_channel_join.scenario

scenario = (
    join_channel,

    # Send a raw QUIT message
    send_stanza("<message from='{jid_one}/{resource_one}' to='{irc_server_one}' type='chat'><body>QUIT bye bye</body></message>"),
    expect_stanza("/presence[@from='#foo%{irc_server_one}/{nick_one}'][@type='unavailable']/muc_user:x/muc_user:status[@code='110']"),
)
