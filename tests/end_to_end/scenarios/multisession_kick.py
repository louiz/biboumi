from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Second user joins, from two resources
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
    expect_unordered(
                     ["/presence[@to='{jid_one}/{resource_one}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant']"],
                     ["/presence[@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
                     ["/presence[@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant']",
                      "/presence[@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:status[@code='110']"],
                     ["/message/subject"]
    ),
    # Second resource
    send_stanza("<presence from='{jid_two}/{resource_two}' to='#foo%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_unordered(
    ["/presence[@to='{jid_two}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']"],
    ["/presence[@to='{jid_two}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_two}']",
     "/presence/muc_user:x/muc_user:status[@code='110']"]
    ),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_two}/{resource_two}']/subject[not(text())]"),

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
            "/presence[@type='unavailable'][@to='{jid_two}/{resource_two}']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
            "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
            "/presence/muc_user:x/muc_user:status[@code='307']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        ["/presence[@type='unavailable']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
        "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
        "/presence/muc_user:x/muc_user:status[@code='307']",
        ],
        [
            "/iq[@id='kick1'][@type='result']"
        ]
    ),
)
