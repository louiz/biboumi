from scenarios import *

scenario = (
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure the IRC server irc.localhost']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:instructions[text()='Edit the form, to configure the settings of the IRC server irc.localhost']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='ports']/dataform:value[text()='6667']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='tls_ports']/dataform:value[text()='6670']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='tls_ports']/dataform:value[text()='6697']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='verify_cert']/dataform:value[text()='true']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='fingerprint']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='throttle_limit']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-private'][@var='pass']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='after_connect_commands']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='max_history_length']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='nick']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='username']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='realname']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_in']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_out']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))
                  ),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='complete'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='ports' />"
                "<field var='tls_ports'><value>6697</value><value>6698</value></field>"
                "<field var='verify_cert'><value>1</value></field>"
                "<field var='fingerprint'><value>12:12:12</value></field>"
                "<field var='pass'><value>coucou</value></field>"
                "<field var='after_connect_commands'><value>first command</value><value>second command</value></field>"
                "<field var='nick'><value>my_nickname</value></field>"
                "<field var='username'><value>username</value></field>"
                "<field var='throttle_limit'><value>42</value></field>"
                "<field var='max_history_length'><value>69</value></field>"
                "<field var='realname'><value>realname</value></field>"
                "<field var='encoding_out'><value>UTF-8</value></field>"
                "<field var='encoding_in'><value>latin-1</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    send_stanza("<iq type='set' id='id3' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure the IRC server irc.localhost']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:instructions[text()='Edit the form, to configure the settings of the IRC server irc.localhost']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='tls_ports']/dataform:value[text()='6697']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='tls_ports']/dataform:value[text()='6698']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='verify_cert']/dataform:value[text()='true']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='fingerprint']/dataform:value[text()='12:12:12']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-private'][@var='pass']/dataform:value[text()='coucou']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='nick']/dataform:value[text()='my_nickname']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='after_connect_commands']/dataform:value[text()='first command']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-multi'][@var='after_connect_commands']/dataform:value[text()='second command']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='username']/dataform:value[text()='username']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='realname']/dataform:value[text()='realname']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='throttle_limit']/dataform:value[text()='42']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='max_history_length']/dataform:value[text()='69']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_in']/dataform:value[text()='latin-1']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_out']/dataform:value[text()='UTF-8']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))
                  ),
    send_stanza("<iq type='set' id='id4' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' action='cancel' node='configure' sessionid='{sessionid}' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='canceled']"),

    # Same thing, but try to empty some values
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))
            ),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='complete'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='pass'><value></value></field>"
                "<field var='after_connect_commands'></field>"
                "<field var='username'><value></value></field>"
                "<field var='realname'><value></value></field>"
                "<field var='throttle_limit'><value></value></field>"
                "<field var='encoding_out'><value></value></field>"
                "<field var='encoding_in'><value></value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    send_stanza("<iq type='set' id='id3' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure the IRC server irc.localhost']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:instructions[text()='Edit the form, to configure the settings of the IRC server irc.localhost']",
                  "!/iq/commands:command/dataform:x/dataform:field[@var='tls_ports']/dataform:value",
                  "!/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='pass']/dataform:value",
                  "!/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='after_connect_commands']/dataform:value",
                  "!/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='username']/dataform:value",
                  "!/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='realname']/dataform:value",
                  "!/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='encoding_in']/dataform:value",
                  "!/iq/commands:command/dataform:x[@type='form']/dataform:field[@var='encoding_out']/dataform:value",
                  "/iq/commands:command/commands:actions/commands:complete",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='throttle_limit']/dataform:value[text()='10']",  # An invalid value sets this field to its default value
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))
                  ),
    send_stanza("<iq type='set' id='id4' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' action='cancel' node='configure' sessionid='{sessionid}' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='canceled']"),

 )
