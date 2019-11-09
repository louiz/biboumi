from scenarios import *

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    send_stanza("<presence type='unavailable' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    # Fixme: what is the purpose of this test? Check that we don’t receive anything here…?

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection(),
    expect_self_join_presence(jid='{jid_one}/{resource_one}', chan="#foo", nick="{nick_one}"),
)
