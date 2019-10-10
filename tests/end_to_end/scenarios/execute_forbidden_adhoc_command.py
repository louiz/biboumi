from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='set' id='command1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='disconnect-user' action='execute' /></iq>"),
    expect_stanza("/iq[@type='error'][@id='command1']/commands:command[@node='disconnect-user']",
                  "/iq/commands:command/commands:error[@type='cancel']/stanza:forbidden"),
)
