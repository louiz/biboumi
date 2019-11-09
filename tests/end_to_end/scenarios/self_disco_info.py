from scenarios import *

scenario = (
    send_stanza("<iq type='get' id='get1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"),
    expect_stanza("/iq[@type='result']/disco_info:query/disco_info:identity[@category='conference'][@type='irc'][@name='Biboumi XMPP-IRC gateway']",
                  "/iq/disco_info:query/disco_info:feature[@var='jabber:iq:version']",
                  "/iq/disco_info:query/disco_info:feature[@var='http://jabber.org/protocol/commands']",
                  "/iq/disco_info:query/disco_info:feature[@var='urn:xmpp:ping']",
                  "/iq/disco_info:query/disco_info:feature[@var='urn:xmpp:mam:2']",
                  "/iq/disco_info:query/disco_info:feature[@var='jabber:iq:version']"),
)
