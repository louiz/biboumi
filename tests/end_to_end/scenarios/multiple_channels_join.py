from functions import send_stanza, expect_stanza

import sequences

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    sequences.handshake(),

    # Join 3 rooms, on the same server, with three different nicks
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_two}' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#baz%{irc_server_one}/{nick_three}'>  <x xmlns='http://jabber.org/protocol/muc'><password>SECRET</password></x></presence>"),

    sequences.connection(),

    # The first nick we specified should be the only one we receive, the rest was ignored
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#foo", nick="{nick_one}"),
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#bar", nick="{nick_one}"),
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#baz", nick="{nick_one}"),
)
