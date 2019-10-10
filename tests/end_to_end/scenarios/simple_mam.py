from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send two channel messages
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']",
                  "/message/stable_id:stanza-id[@by='#foo%{irc_server_one}'][@id]"),

    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou 2</body></message>"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou 2']"),

    # Retrieve the complete archive
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id1'><query xmlns='urn:xmpp:mam:2' queryid='qid1' /></iq>"),

    expect_stanza("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou']"),
    expect_stanza("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 2']"),

    expect_stanza("/iq[@type='result'][@id='id1'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin/rms:set/rsm:last",
                  "/iq/mam:fin/rsm:set/rsm:first",
                  "/iq/mam:fin[@complete='true']"),

    # Retrieve an empty archive by specifying an early “end” date
    send_stanza("""<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id2'>
                <query xmlns='urn:xmpp:mam:2' queryid='qid2'>
                <x xmlns='jabber:x:data' type='submit'>
                <field var='FORM_TYPE' type='hidden'> <value>urn:xmpp:mam:2</value></field>
                <field var='end'><value>2000-06-07T00:00:00Z</value></field>
                </x>
                </query></iq>"""),

    expect_stanza("/iq[@type='result'][@id='id2'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin[@complete='true']/rsm:set"),

    # Retrieve an empty archive by specifying a late “start” date
    # (note that this test will break in ~1000 years)
    send_stanza("""<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id3'>
                <query xmlns='urn:xmpp:mam:2' queryid='qid3'>
                <x xmlns='jabber:x:data' type='submit'>
                <field var='FORM_TYPE' type='hidden'> <value>urn:xmpp:mam:2</value></field>
                <field var='start'><value>3016-06-07T00:00:00Z</value></field>
                </x>
                </query></iq>"""),

    expect_stanza("/iq[@type='result'][@id='id3'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin[@complete='true']/rsm:set"),

    # Retrieve the whole archive, but limit the response to one elemet
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id4'><query xmlns='urn:xmpp:mam:2' queryid='qid4'><set xmlns='http://jabber.org/protocol/rsm'><max>1</max></set></query></iq>"),

    expect_stanza("/message/mam:result[@queryid='qid4']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid4']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou']"),

    expect_stanza("/iq[@type='result'][@id='id4'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "!/iq/mam:fin[@complete='true']/rsm:set"),
)
