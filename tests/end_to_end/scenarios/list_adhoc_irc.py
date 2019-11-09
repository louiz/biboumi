from scenarios import *

scenario = (
    send_stanza("<iq type='get' id='idwhatever' from='{jid_one}/{resource_one}' to='{irc_host_one}@{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                  "/iq/disco_items:query/disco_items:item[2]",
                  "!/iq/disco_items:query/disco_items:item[3]"),
)
