from scenarios import *

scenario = (
    # Disable the throttling, otherwise it’s way too long
    send_stanza("<iq type='set' id='id1' from='{jid_one}/{resource_one}' to='{irc_server_one}'><command xmlns='http://jabber.org/protocol/commands' node='configure' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='configure']", "sessionid"))),
    send_stanza("<iq type='set' id='id2' from='{jid_one}/{resource_one}' to='{irc_server_one}'>"
                "<command xmlns='http://jabber.org/protocol/commands' node='configure' sessionid='{sessionid}' action='next'>"
                "<x xmlns='jabber:x:data' type='submit'>"
                "<field var='ports'><value>6667</value></field>"
                "<field var='tls_ports'><value>6697</value><value>6670</value></field>"
                "<field var='throttle_limit'><value>9999</value></field>"
                "</x></command></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='configure'][@status='completed']/commands:note[@type='info'][text()='Configuration successfully applied.']"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]",
                  after = save_value("counter", lambda x: 0)),
    (
        send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>{counter}</body></message>"),
        expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='{counter}']",
                      after = save_value("counter", lambda stanza: str(1 + int(extract_text("/message/body", stanza))))),
    ) * 150,

    # Retrieve the archive, without any restriction
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id1'><query xmlns='urn:xmpp:mam:2' queryid='qid1' /></iq>"),
    expect_stanza("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='0']"),
    # followed by 98 more messages
    (
        expect_stanza("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                      "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body"),
    ) * 98,

    # and finally the message "99"
    expect_stanza("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='99']",
                  after = save_value("last_uuid", extract_attribute("/message/mam:result", "id"))),

    # And it should not be marked as complete
    expect_stanza("/iq[@type='result'][@id='id1'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin/rsm:set/rsm:last[text()='{last_uuid}']",
                  "!/iq//mam:fin[@complete='true']",
                  "/iq//mam:fin"),

    # Retrieve the next page, using the “after” thingy
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id2'><query xmlns='urn:xmpp:mam:2' queryid='qid2' ><set xmlns='http://jabber.org/protocol/rsm'><after>{last_uuid}</after></set></query></iq>"),

    expect_stanza("/message/mam:result[@queryid='qid2']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid2']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='100']"),
    (
        expect_stanza("/message/mam:result[@queryid='qid2']/forward:forwarded/delay:delay",
                      "/message/mam:result[@queryid='qid2']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body"),
    ) * 48,
    expect_stanza("/message/mam:result[@queryid='qid2']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid2']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='149']",
                  after = save_value("last_uuid", extract_attribute("/message/mam:result", "id"))),
    expect_stanza("/iq[@type='result'][@id='id2'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin/rsm:set/rsm:last[text()='{last_uuid}']",
                  "/iq//mam:fin[@complete='true']",
                  "/iq//mam:fin"),

    # Send a request with a non-existing ID set as the “after” value.
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id3'><query xmlns='urn:xmpp:mam:2' queryid='qid3' ><set xmlns='http://jabber.org/protocol/rsm'><after>DUMMY_ID</after></set></query></iq>"),
    expect_stanza("/iq[@id='id3'][@type='error']/error[@type='cancel']/stanza:item-not-found"),

    # Request the last page just BEFORE the last message in the archive
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id3'><query xmlns='urn:xmpp:mam:2' queryid='qid3' ><set xmlns='http://jabber.org/protocol/rsm'><before></before></set></query></iq>"),

    expect_stanza("/message/mam:result[@queryid='qid3']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid3']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='50']"),
    (
        expect_stanza("/message/mam:result[@queryid='qid3']/forward:forwarded/delay:delay",
                      "/message/mam:result[@queryid='qid3']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body"),
    ) * 98,
    expect_stanza("/message/mam:result[@queryid='qid3']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid3']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='149']",
                  after = save_value("last_uuid", extract_attribute("/message/mam:result", "id"))),
    expect_stanza("/iq[@type='result'][@id='id3'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin/rsm:set/rsm:last[text()='{last_uuid}']",
                  "!/iq//mam:fin[@complete='true']",
                  "/iq//mam:fin"),

    # Do the same thing, but with a limit value.
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id4'><query xmlns='urn:xmpp:mam:2' queryid='qid4' ><set xmlns='http://jabber.org/protocol/rsm'><before>{last_uuid}</before><max>2</max></set></query></iq>"),
    expect_stanza("/message/mam:result[@queryid='qid4']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid4']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='147']"),
    expect_stanza("/message/mam:result[@queryid='qid4']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid4']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='148']",
                  after = save_value("last_uuid", extract_attribute("/message/mam:result", "id"))),
    expect_stanza("/iq[@type='result'][@id='id4'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin/rsm:set/rsm:last[text()='{last_uuid}']",
                  "!/iq/mam:fin[@complete='true']"),

    # Test if everything is fine even with weird max value: 0
    send_stanza("<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id5'><query xmlns='urn:xmpp:mam:2' queryid='qid5' ><set xmlns='http://jabber.org/protocol/rsm'><before></before><max>0</max></set></query></iq>"),

    expect_stanza("/iq[@type='result'][@id='id5'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "!/iq/mam:fin[@complete='true']"),
)
