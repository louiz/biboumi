from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,
    
    # Send two channel messages
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']",
                  "/message/stable_id:stanza-id[@by='#foo%{irc_server_one}'][@id]"),

    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou 2</body></message>"),
    # Record the current time
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou 2']",
                  after = save_current_timestamp_plus_delta("first_timestamp", datetime.timedelta(seconds=1))),

    # Wait two seconds before sending two new messages
    sleep_for(2),
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou 3</body></message>"),
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou 4</body></message>"),
    expect_stanza("/message[@type='groupchat']/body[text()='coucou 3']"),
    expect_stanza("/message[@type='groupchat']/body[text()='coucou 4']",
                  after = save_current_timestamp_plus_delta("second_timestamp", datetime.timedelta(seconds=1))),

    # Retrieve the archive, after our saved datetime
    send_stanza("""<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id8'>
                          <query xmlns='urn:xmpp:mam:2' queryid='qid16'>
                            <x type='submit' xmlns='jabber:x:data'>
                             <field var='FORM_TYPE' xmlns='jabber:x:data'><value xmlns='jabber:x:data'>urn:xmpp:mam:2</value></field>
                             <field var='start' xmlns='jabber:x:data'><value xmlns='jabber:x:data'>{first_timestamp}</value></field>
                             <field var='end' xmlns='jabber:x:data'><value xmlns='jabber:x:data'>{second_timestamp}</value></field>
                            </x>
                          </query>
                         </iq>"""),

    expect_stanza("/message/mam:result[@queryid='qid16']/forward:forwarded/delay:delay",
                  "/message/mam:result/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 3']"),

    expect_stanza("/message/mam:result[@queryid='qid16']/forward:forwarded/delay:delay",
                  "/message/mam:result/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 4']"),

    expect_stanza("/iq[@type='result'][@id='id8'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin[@complete='true']/rsm:set"),

    # Try the same thing, but only with the 'start' value, omitting the end
    send_stanza("""<iq from='{jid_one}/{resource_one}' id='id888' to='#foo%{irc_server_one}' type='set'>
                      <query queryid='qid17' xmlns='urn:xmpp:mam:2'>
                          <x type='submit' xmlns='jabber:x:data'>
                              <field type='hidden' var='FORM_TYPE' xmlns='jabber:x:data'><value xmlns='jabber:x:data'>urn:xmpp:mam:2</value></field>
                              <field var='start' xmlns='jabber:x:data'><value xmlns='jabber:x:data'>{first_timestamp}</value></field>
                          </x>
                       </query>
                   </iq>"""),

    expect_stanza("/message/mam:result[@queryid='qid17']/forward:forwarded/delay:delay",
                  "/message/mam:result/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 3']"),

    expect_stanza("/message/mam:result[@queryid='qid17']/forward:forwarded/delay:delay",
                  "/message/mam:result/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 4']"),

    expect_stanza("/iq[@type='result'][@id='id888'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']",
                  "/iq/mam:fin[@complete='true']/rsm:set"),

)
