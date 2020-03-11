from scenarios import *

import scenarios.simple_channel_join

scenario = (
    scenarios.simple_channel_join.scenario,

    # First user sets the topic
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><subject>TOPIC TEST</subject></message>"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat'][@to='{jid_one}/{resource_one}']/subject[text()='TOPIC TEST']"),

    # Second user joins
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
    expect_unordered(
        [
            "/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost'][@role='participant']"
        ],
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"
        ],
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost'][@role='participant']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        [
            "/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/subject[text()='TOPIC TEST']"
        ]
    )
)
