from scenarios import *

conf = 'fixed_server'

scenario = (
    send_stanza("<presence type='subscribe' from='{jid_one}/{resource_one}' to='{biboumi_host}' id='sub1' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence[@to='{jid_one}'][@from='{biboumi_host}'][@type='subscribed']")
)
