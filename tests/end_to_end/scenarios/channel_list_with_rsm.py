from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/message/body[text()='Mode #bar [+nt] by {irc_host_one}']"),
    expect_stanza("/presence"),
    expect_stanza("/message[@from='#bar%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#coucou%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/message/body[text()='Mode #coucou [+nt] by {irc_host_one}']"),
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
                  "/iq/disco_items:query/disco_items:item[@jid='#bar%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#coucou%{irc_server_one}']",
                  "/iq/disco_items:query/rsm:set/rsm:first[text()='#bar%{irc_server_one}'][@index='0']",
                  "/iq/disco_items:query/rsm:set/rsm:last[text()='#coucou%{irc_server_one}']"),

    # Ask for 12 (of 3) items. We get the whole list, and thus we have the count included.
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>12</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "/iq/disco_items:query/disco_items:item[@jid='#bar%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#coucou%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#foo%{irc_server_one}']",
                  "/iq/disco_items:query/rsm:set/rsm:first[text()='#bar%{irc_server_one}'][@index='0']",
                  "/iq/disco_items:query/rsm:set/rsm:last[text()='#foo%{irc_server_one}']",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"),

    # Ask for 1 item, AFTER the first item (so,
    # the second). Since we don’t invalidate the cache
    # with this request, we should have the count
    # included.
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#bar%{irc_server_one}</after><max>1</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "/iq/disco_items:query/disco_items:item[@jid='#coucou%{irc_server_one}']",
                  "/iq/disco_items:query/rsm:set/rsm:first[text()='#coucou%{irc_server_one}'][@index='1']",
                  "/iq/disco_items:query/rsm:set/rsm:last[text()='#coucou%{irc_server_one}']",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"),

    # Ask for 1 item, AFTER the second item (so,
    # the third).
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#coucou%{irc_server_one}</after><max>1</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "/iq/disco_items:query/disco_items:item[@jid='#foo%{irc_server_one}']",
                  "/iq/disco_items:query/rsm:set/rsm:first[text()='#foo%{irc_server_one}'][@index='2']",
                  "/iq/disco_items:query/rsm:set/rsm:last[text()='#foo%{irc_server_one}']",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"),

    # Ask for 1 item, AFTER the third item (so,
    # the fourth). Since it doesn't exist, we get 0 item
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#foo%{irc_server_one}</after><max>1</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"),
)
