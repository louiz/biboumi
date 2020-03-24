from scenarios import *

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#aaa%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bbb%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#ccc%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#ddd%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#eee%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#fff%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#ggg%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#hhh%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#iii%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#jjj%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence"),
    expect_stanza("/message"),

    send_stanza("<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>3</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item[@jid])=3",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='0']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  after = save_value("last", lambda stanza: extract_text("/iq/disco_items:query/rsm:set/rsm:last", stanza))),

    send_stanza("<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>{last}</after><max>3</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item[@jid])=3",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='3']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  after = save_value("last", lambda stanza: extract_text("/iq/disco_items:query/rsm:set/rsm:last", stanza))),

    send_stanza("<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>{last}</after><max>3</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item[@jid])=3",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='6']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  after = save_value("last", lambda stanza: extract_text("/iq/disco_items:query/rsm:set/rsm:last", stanza))),

    send_stanza("<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>{last}</after><max>3</max></set></query></iq>"),
    expect_stanza("/iq[@type='result']/disco_items:query",
                  "count(/iq/disco_items:query/disco_items:item[@jid])=1",
                  "/iq/disco_items:query/rsm:set/rsm:first[@index='9']",
                  "/iq/disco_items:query/rsm:set/rsm:last",
                  "/iq/disco_items:query/rsm:set/rsm:count[text()='10']"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#aaa%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bbb%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#ccc%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#ddd%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#eee%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#fff%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#ggg%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#hhh%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#iii%{irc_server_one}/{nick_one}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#jjj%{irc_server_one}/{nick_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unavailable']"),
)
