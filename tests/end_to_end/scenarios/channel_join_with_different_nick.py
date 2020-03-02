from scenarios import *

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "{nick_one}"),

    # The same resource joins a different channel with a different nick
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_two}' />"),
    # We must receive a join presence in response, without any nick change (nick_two) must be ignored
    expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#bar", nick = "{nick_one}"),

    # An different resource joins the same channel, with a different nick
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_two}' />"),
    # We must receive a join presence in response, without any nick change (nick_two) must be ignored
    expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']"),
    expect_stanza("/message/subject"),
)
