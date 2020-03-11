from scenarios import *

scenario = (
    # Disable the throttling because the test is based on timings
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
    expect_stanza("/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

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

    # join some other channel, to stay connected to the server even after leaving #foo
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#DUMMY%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message/subject"),

    # Leave #foo
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    sleep_for(0.2),

    # Rejoin #foo, with some history limit
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}'><x xmlns='http://jabber.org/protocol/muc'><history maxchars='0'/></x></presence>"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message/subject"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    sleep_for(0.2),

    # Rejoin #foo, with some history limit
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}'><x xmlns='http://jabber.org/protocol/muc'><history maxstanzas='3'/></x></presence>"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 2']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 3']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 4']"),
    expect_stanza("/message/subject"),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    # Rejoin #foo, with some history limit
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}'><x xmlns='http://jabber.org/protocol/muc'><history since='{first_timestamp}'/></x></presence>"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 3']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 4']"),
    expect_stanza("/message/subject"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    # Rejoin #foo, with some history limit
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}'><x xmlns='http://jabber.org/protocol/muc'><history seconds='1'/></x></presence>"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 3']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 4']"),
    expect_stanza("/message/subject"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),

    # Rejoin #foo, with some history limit
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}'><x xmlns='http://jabber.org/protocol/muc'><history seconds='5'/></x></presence>"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou']"),                     expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 2']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 3']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/body[text()='coucou 4']"),
    expect_stanza("/message/subject"),
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']"),
)
