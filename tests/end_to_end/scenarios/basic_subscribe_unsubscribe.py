from scenarios import *

scenario = (
    sequences.handshake(),

    # Mutual subscription exchange
    send_stanza("<presence from='{jid_one}' to='{biboumi_host}' type='subscribe' id='subid1' />"),
    expect_stanza("/presence[@type='subscribed'][@id='subid1']"),

    # Get the current presence of the biboumi gateway
    expect_stanza("/presence"),

    expect_stanza("/presence[@type='subscribe']"),
    send_stanza("<presence from='{jid_one}' to='{biboumi_host}' type='subscribed' />"),

    # Unsubscribe
    send_stanza("<presence from='{jid_one}' to='{biboumi_host}' type='unsubscribe' id='unsubid1' />"),
    expect_stanza("/presence[@type='unavailable']"),
    expect_stanza("/presence[@type='unsubscribed']"),
    expect_stanza("/presence[@type='unsubscribe']"),
    send_stanza("<presence from='{jid_one}' to='{biboumi_host}' type='unavailable' />"),
    send_stanza("<presence from='{jid_one}' to='{biboumi_host}' type='unsubscribed' />"),
)
