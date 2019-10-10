from scenarios import *

conf = 'fixed_server'

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='global-configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='global-configure'][@sessionid][@status='executing']",
                            "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure some global default settings.']",
                            "/iq/commands:command/dataform:x[@type='form']/dataform:instructions[text()='Edit the form, to configure your global settings for the component.']",
                            "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='max_history_length']/dataform:value[text()='20']",
                            "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='record_history']/dataform:value[text()='true']",
                            "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='persistent']/dataform:value[text()='false']",
                            "/iq/commands:command/commands:actions/commands:complete",
                            after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='global-configure']", "sessionid"))),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='global-configure' sessionid='{sessionid}' action='complete'><x xmlns='jabber:x:data' type='submit'><field var='record_history'><value>0</value></field><field var='max_history_length'><value>42</value></field></x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='global-configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    send_stanza("<iq type='set' id='id3' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='global-configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='global-configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure some global default settings.']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:instructions[text()='Edit the form, to configure your global settings for the component.']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='max_history_length']/dataform:value[text()='42']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='record_history']/dataform:value[text()='false']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='persistent']/dataform:value[text()='false']",
                  "/iq/commands:command/commands:actions/commands:complete",
                 after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='global-configure']", "sessionid"))),
    send_stanza("<iq type='set' id='id4' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' action='cancel' node='global-configure' sessionid='{sessionid}' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='global-configure'][@status='canceled']"),

    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='server-configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='server-configure'][@sessionid][@status='executing']"),
)
