import sequences

from functions import send_stanza, expect_stanza, expect_unordered

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    sequences.handshake(),
    # First user joins
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#foo", nick="{nick_one}"),

    # Second user joins
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
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
