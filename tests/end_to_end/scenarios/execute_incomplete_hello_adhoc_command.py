from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='set' id='hello-command1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='hello' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='hello'][@sessionid][@status='executing']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='hello']", "sessionid"))
                  ),
    send_stanza("<iq type='set' id='hello-command2' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='hello' sessionid='{sessionid}' action='complete'><x xmlns='jabber:x:data' type='submit'></x></command></iq>"),
    expect_stanza("/iq[@type='error']")
)
