from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send a version request to ourself
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='first_version' to='#foo%{irc_server_one}/{nick_one}'><query xmlns='jabber:iq:version' /></iq>"),
    # We receive our own request,
    expect_stanza("/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to='{jid_one}/{resource_one}']",
                  after = save_value("id", extract_attribute("/iq", 'id'))),
    # Respond to the request, and receive our own response
    send_stanza("<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='{id}' from='{jid_one}/{resource_one}'><query xmlns='jabber:iq:version'><name>e2e test</name><version>1.0</version><os>Fedora</os></query></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_version']/version:query/version:name[text()='e2e test (through the biboumi gateway) 1.0 Fedora']"),

    # Now join the same room, from the same bare JID, behind the same nick
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

    # And re-send a self ping
    send_stanza("<iq type='get' from='{jid_one}/{resource_two}' id='second_version' to='#foo%{irc_server_one}/{nick_one}'><query xmlns='jabber:iq:version' /></iq>"),
    # We receive our own request. Note that we don't know the `to` value, it could be one of our two resources.
    expect_stanza("/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to]",
                  after = (save_value("to", extract_attribute("/iq", "to")),
                           save_value("id", extract_attribute("/iq", "id")))),
    # Respond to the request, using the extracted 'to' value as our 'from'
    send_stanza("<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='{id}' from='{to}'><query xmlns='jabber:iq:version'><name>e2e test</name><version>1.0</version><os>Fedora</os></query></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_two}'][@id='second_version']"),

    # And do exactly the same thing, but initiated by the other resource
    send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='second_version' to='#foo%{irc_server_one}/{nick_one}'><query xmlns='jabber:iq:version' /></iq>"),
    expect_stanza("/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to]",
                  after = (save_value("to", extract_attribute("/iq", "to")),
                           save_value("id", extract_attribute("/iq", "id")))),
    # Respond to the request, using the extracted 'to' value as our 'from'
    send_stanza("<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='{id}' from='{to}'><query xmlns='jabber:iq:version'><name>e2e test</name><version>1.0</version><os>Fedora</os></query></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='second_version']"),
)
