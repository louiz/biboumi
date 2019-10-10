from scenarios import *

conf = "fixed_server"

scenario = (
    scenarios.simple_channel_join_fixed.scenario,
    
    # Send a ping to ourself
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='first_ping' to='#foo@{biboumi_host}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
    expect_stanza("/iq[@from='#foo@{biboumi_host}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_ping']"),
)
