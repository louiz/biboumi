from scenarios import *

scenario = (
    sequences.handshake(),

    # Configure the throttle option with an incorrect value
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='next'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='throttle_limit'><value>bleh</value></field>"
                "<field var='max_history_length'><value>bleh</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']"),

    # These options should have their default value
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='throttle_limit']/dataform:value[text()='10']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='max_history_length']/dataform:value[text()='20']"),
)
