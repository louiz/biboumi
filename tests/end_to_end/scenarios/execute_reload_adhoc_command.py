from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='set' id='ping-command1' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='reload' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='reload'][@status='completed']/commands:note[@type='info'][text()='Configuration reloaded.']"),
)
