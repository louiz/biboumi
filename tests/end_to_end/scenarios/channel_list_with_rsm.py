from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message[@from='#bar%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#coucou%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message[@from='#coucou%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

    # Ask for 0 item
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>0</max></set></query></iq>"),

    # Get 0 item
    expect_stanza("/iq[@type='result']/disco_items:query"),

    # Ask for 2 (of 3) items We don’t have the count,
    # because biboumi doesn’t have the complete list when
    # it sends us the 2 items
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>2</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item[@jid])=2",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='0']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  "!/iq/disco_items:query/rsm:set/rsm:count"),

    # Ask for 12 (of 3) items. We get the whole list, and thus we have the count included.
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>12</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item[@jid])=3",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='0']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']",
                  after = save_value("first", lambda stanza: extract_text("/iq/disco_items:query/rsm:set/rsm:first", stanza))),

    # Ask for 1 item, AFTER the first item (so,
    # the second). Since we don’t invalidate the cache
    # with this request, we should have the count
    # included.
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>{first}</after><max>1</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item)=1",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='1']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']",
                  after = save_value("second", lambda stanza: extract_text("/iq/disco_items:query/rsm:set/rsm:first", stanza))),

    # Ask for 1 item, AFTER the second item (so,
    # the third).
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>{second}</after><max>1</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item)=1",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='2']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']",
                  after = save_value("third", lambda stanza: extract_text("/iq/disco_items:query/rsm:set/rsm:first", stanza))),

    # Ask for 1 item, AFTER the third item (so,
    # the fourth). Since it doesn't exist, we get 0 item
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>{third}</after><max>1</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item)=0",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"),
)
