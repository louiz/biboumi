from scenarios import *

def incr_counter():
    counter = -1
    def f(stanza):
        nonlocal counter
        counter += 1
        return counter
    return f

counter = incr_counter()

scenario = (
    # Disable the throttling, otherwise it’s way too long
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='next'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='ports'><value>6667</value></field>"
                "<field var='tls_ports'><value>6697</value><value>6670</value></field>"
                "<field var='throttle_limit'><value>9999</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']",
                  after = save_value("counter", counter)),


    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection(),

    scenarios.simple_channel_join.expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "{nick_one}"),


    (
        send_stanza("<presence from='{jid_one}/{resource_one}' to='#{counter}%{irc_server_one}/{nick_one}' />"),
        expect_stanza("/message"),
        expect_stanza("/presence",
                      after = save_value("counter", counter)),
        expect_stanza("/message"),
    ) * 110,

    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"),
    # charybdis sends the list in alphabetic order, so #foo is the last, and #99 is after #120
    expect_stanza("/iq/disco_items:query/disco_items:item[@jid='#0%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#1%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#109%{irc_server_one}']",
                  "/iq/disco_items:query/disco_items:item[@jid='#9%{irc_server_one}']",
                  "!/iq/disco_items:query/disco_items:item[@jid='#foo%{irc_server_one}']",
                  "!/iq/disco_items:query/disco_items:item[@jid='#99%{irc_server_one}']",
                  "!/iq/disco_items:query/disco_items:item[@jid='#90%{irc_server_one}']"),
)
