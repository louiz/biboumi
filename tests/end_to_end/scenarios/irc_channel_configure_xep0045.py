from scenarios import *

scenario = (
    sequences.handshake(),
    send_stanza("<iq type='get' id='id1' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><query xmlns='http://jabber.org/protocol/muc#owner'/></iq>"),
    expect_stanza("/iq[@type='result']/muc_owner:query",
                  "/iq/muc_owner:query/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_in']",
                  "/iq/muc_owner:query/dataform:x[@type='form']/dataform:field[@type='text-single'][@var='encoding_out']",

                  ),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'>"
                "<query xmlns='http://jabber.org/protocol/muc#owner'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='ports' />"
                "<field var='encoding_out'><value>UTF-8</value></field>"
                "<field var='encoding_in'><value>latin-1</value></field>"
                "</x></query></iq>"),
    expect_stanza("/iq[@type='result']"),
    send_stanza("<iq type='set' id='id3' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><query xmlns='http://jabber.org/protocol/muc#owner'>    <x xmlns='jabber:x:data' type='cancel'/></query></iq>"),
    expect_stanza("/iq[@type='result']"),
)
