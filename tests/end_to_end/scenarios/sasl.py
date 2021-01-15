from scenarios import *

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/RegisteredUser' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection(),

    simple_channel_join.expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "RegisteredUser"),

    # Create an account by talking to nickserv directly
    send_stanza("<message from='{jid_one}/{resource_one}' to='nickserv%{irc_server_one}' type='chat'><body>register P4SSW0RD</body></message>"),
    expect_stanza("/message/body[text()='Account created']"),
    expect_stanza("/message/body[text()=\"You're now logged in as RegisteredUser\"]"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    # Configure an sasl password
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-private'][@var='sasl_password']",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))),

    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='complete'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='sasl_password'><value>P4SSW0RD</value></field>"
                "<field var='ports'><value>6667</value></field>"
                "<field var='nick'><value>RegisteredUser</value></field>"
                "<field var='tls_ports'><value>6697</value><value>6670</value></field>"
                "<field var='throttle_limit'><value>9999</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    # Joining a channel with the associated nick will work
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/ignored' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection(login="RegisteredUser"),
    expect_stanza("/presence"),
    expect_stanza("/message/subject"),

    # Leave the channel and disconnect from the server to try again differently
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/RegisteredUser' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    # Leave the same password, but remove the Nick
    send_stanza("<iq type='set' id='id3' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-private'][@var='sasl_password']",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))),

    send_stanza("<iq type='set' id='id4' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='complete'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='sasl_password'><value>P4SSW0RD</value></field>"
                "<field var='ports'><value>6667</value></field>"
                "<field var='nick'><value></value></field>"
                "<field var='tls_ports'><value>6697</value><value>6670</value></field>"
                "<field var='throttle_limit'><value>9999</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    # Joining a channel with the associated nick will work, it will use the one from our <presence/>
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/RegisteredUser' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection(login="RegisteredUser"),
    expect_stanza("/presence"),
    expect_stanza("/message/subject"),

    # Leave the channel and disconnect from the server to try again differently
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/RegisteredUser' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    # Configure an INCORRECT password
    send_stanza("<iq type='set' id='id5' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-private'][@var='sasl_password']",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))),


    send_stanza("<iq type='set' id='id6' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='complete'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='sasl_password'><value>wrong wrong wrong</value></field>"
                "<field var='ports'><value>6667</value></field>"
                "<field var='nick'><value>RegisteredUser</value></field>"
                "<field var='tls_ports'><value>6697</value><value>6670</value></field>"
                "<field var='throttle_limit'><value>9999</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/ignored' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    # Here, the 6 connecting… connected messages from the connection attempt
    expect_stanza("/message"),
    expect_stanza("/message"),
    expect_stanza("/message"),
    expect_stanza("/message"),
    expect_stanza("/message"),
    expect_stanza("/message"),
    expect_stanza("/presence[@type='error'][@from='#foo%{irc_server_one}/RegisteredUser']"),
    expect_stanza("/message/body[text()='ERROR: Quit: SASL authentication failed: Invalid account credentials']"),
    expect_stanza("/message/body[text()='ERROR: Connection closed.']"),
)
