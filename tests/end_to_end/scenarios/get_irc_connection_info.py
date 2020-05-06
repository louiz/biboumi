from scenarios import *

scenario = (
    send_stanza("<iq type='set' id='command1' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='get-irc-connection-info' action='execute' /></iq>"),
    expect_stanza("/iq/commands:command/commands:note[text()='You are not connected to the IRC server irc.localhost']"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    simple_channel_join.expect_self_join_presence(),

    send_stanza("<iq type='set' id='command2' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='get-irc-connection-info' action='execute' /></iq>"),
    expect_stanza(r"/iq/commands:command/commands:note[re:test(text(), 'Connected to IRC server irc.localhost on port 6667 since \d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d \(\d+ seconds ago\)\.\n#foo from 1 resource: {resource_one}.*')]"),
)
