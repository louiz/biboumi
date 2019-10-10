from scenarios import *

scenario = (
    scenarios.channel_join_with_two_users.scenario,
    # demonstrate bug https://lab.louiz.org/louiz/biboumi/issues/3291
    # First user joins an other channel
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_one}' />"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@type='groupchat']/subject"),

    # Second user joins
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#bar%{irc_server_one}/{nick_two}' />"),
    expect_unordered(
        ["/presence[@to='{jid_one}/{resource_one}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant']"],
        [
            "/presence[@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant']",
                      "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        ["/presence[@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
        ["/message/subject"]
    ),

    # Moderator kicks participant
    send_stanza("<iq id='kick1' to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item nick='{nick_two}' role='none'><reason>reported</reason></item></query></iq>"),
    expect_unordered(
        [
            "/presence[@type='unavailable'][@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
            "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
            "/presence/muc_user:x/muc_user:status[@code='307']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        [
            "/presence[@type='unavailable'][@to='{jid_one}/{resource_one}']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
            "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
            "/presence/muc_user:x/muc_user:status[@code='307']",
        ],
        ["/iq[@id='kick1'][@type='result']"]
    ),

    # Bug 3291, suite. We must not receive any presence from #foo, here
    send_stanza("<message from='{jid_two}/{resource_one}' to='{irc_server_one}' type='chat'><body>QUIT bye bye</body></message>"),
    expect_unordered(
        ["/presence[@from='#bar%{irc_server_one}/{nick_two}'][@to='{jid_one}/{resource_one}']"],
        ["/presence[@from='#bar%{irc_server_one}/{nick_two}'][@to='{jid_two}/{resource_one}']"],
        ["/message"],
        ["/message"],
    ),
)
