from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='set' id='ping-command1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='ping' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='ping'][@status='completed']/commands:note[@type='info'][text()='Pong']")
)
