from scenarios import *

import scenarios.simple_channel_join

scenario = (
    scenarios.simple_channel_join.scenario,

    # Set a password in the room, by using /mode +k
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>/mode +k SECRET</body></message>"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='Mode #foo [+k SECRET] by {nick_one}']"),

    # Second user tries to join, without a password (error ensues)
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}'><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
    expect_stanza("/message/body[text()='{irc_host_one}: #foo: Cannot join channel (+k) - bad key']"),
    expect_stanza("/presence[@type='error'][@from='#foo%{irc_server_one}/{nick_two}']/error[@type='auth']/stanza:not-authorized"),

    # Second user joins, with the correct password (success)
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}'>  <x xmlns='http://jabber.org/protocol/muc'><password>SECRET</password></x></presence>"),
    expect_unordered(
        [
            "/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost']"
        ],
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"
        ],
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost'][@role='participant']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        [
            "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"
        ]
    )
)
