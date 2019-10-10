from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<presence type='subscribe' from='{jid_one}/{resource_one}' to='{irc_server_one}' id='sub1' />"),
    expect_stanza("/presence[@to='{jid_one}'][@from='{irc_server_one}'][@type='subscribed']"),
)
