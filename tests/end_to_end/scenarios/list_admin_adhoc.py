from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='get' id='idwhatever' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                  "/iq/disco_items:query/disco_items:item[@node='configure']",
                  "/iq/disco_items:query/disco_items:item[6]",
                  "!/iq/disco_items:query/disco_items:item[7]"),
)
