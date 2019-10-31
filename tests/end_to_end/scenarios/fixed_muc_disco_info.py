from scenarios import *

conf = 'fixed_server'

scenario = (
    sequences.handshake(),
    
    send_stanza("<iq from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' id='1' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"),
    expect_stanza("/iq[@from='#foo@{biboumi_host}'][@to='{jid_one}/{resource_one}'][@type='result']/disco_info:query",
                  "/iq[@type='result']/disco_info:query/disco_info:identity[@category='conference'][@type='irc'][@name='#foo on {irc_host_one}']",
                  "/iq/disco_info:query/disco_info:feature[@var='jabber:iq:version']",
                  "/iq/disco_info:query/disco_info:feature[@var='http://jabber.org/protocol/commands']",
                  "/iq/disco_info:query/disco_info:feature[@var='urn:xmpp:ping']",
                  "/iq/disco_info:query/disco_info:feature[@var='urn:xmpp:mam:2']",
                  "/iq/disco_info:query/disco_info:feature[@var='jabber:iq:version']"),
)
