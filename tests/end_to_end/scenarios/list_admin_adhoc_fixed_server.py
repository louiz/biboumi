from scenarios import *

conf = "fixed_server"

scenario = (
    send_stanza("<iq type='get' id='idwhatever' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                  "/iq/disco_items:query/disco_items:item[@node='global-configure']",
                  "/iq/disco_items:query/disco_items:item[@node='server-configure']",
                  "/iq/disco_items:query/disco_items:item[8]",
                  "!/iq/disco_items:query/disco_items:item[9]"),
)
