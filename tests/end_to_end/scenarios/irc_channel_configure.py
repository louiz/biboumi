from scenarios import *

scenario = (
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute'><dummy/></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_in']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_out']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='list-single'][@var='record_history']/dataform:value[text()='unset']",
                  "!/iq/commands:command/commands:dummy",

                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))
                  ),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='complete'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='ports' />"
                "<field var='encoding_out'><value>UTF-8</value></field>"
                "<field var='encoding_in'><value>latin-1</value></field>"
                "<field var='record_history'><value>true</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    send_stanza("<iq type='set' id='id3' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure the IRC channel #foo on server irc.localhost']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_in']/dataform:value[text()='latin-1']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_out']/dataform:value[text()='UTF-8']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='list-single'][@var='record_history']/dataform:value[text()='true']",
                  "/iq/commands:command/commands:actions/commands:complete",

                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))
                  ),
    send_stanza("<iq type='set' id='id4' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' action='cancel' node='configure' sessionid='{sessionid}' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='canceled']"),
)
