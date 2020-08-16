from scenarios import *

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    # Join 3 rooms, on the same server, with three different nicks
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#baz%{irc_server_one}/{nick_three}'>  <x xmlns='http://jabber.org/protocol/muc'><password>SECRET</password></x></presence>"),

    sequences.connection(),

    # The first nick we specified should be the only one we receive, the rest was ignored
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#foo", nick="{nick_one}"),
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#bar", nick="{nick_one}"),
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#baz", nick="{nick_one}"),

    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><subject>Le topic</subject></message>"),
    expect_stanza("/message"),

)

