from scenarios import *

scenario = (
    scenarios.multiple_channels_join.scenario,

    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']",
                  "/iq/disco_items:query/rsm:set/rsm:first",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  "/iq/disco_items:query/disco_items:item[@jid='#foo%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#bar%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#baz%{irc_server_one}']"),
)
