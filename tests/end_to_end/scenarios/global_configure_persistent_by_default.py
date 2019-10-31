from scenarios import *

conf='persistent_by_default'

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@sessionid][@status='executing']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure some global default settings.']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:instructions[text()='Edit the form, to configure your global settings for the component.']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='max_history_length']/dataform:value[text()='20']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='record_history']/dataform:value[text()='true']",
                  "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='boolean'][@var='persistent']/dataform:value[text()='true']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  ),
)
