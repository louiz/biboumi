from scenarios import *

from scenarios.simple_channel_join import expect_self_join_presence

scenario = (
    sequences.handshake(),

    # Resource one joins a channel
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection(),
    expect_self_join_presence(jid = '{jid_one}/{resource_one}', chan = "#foo", nick = "{nick_one}"),

    # The other resources joins the same room, with the same nick
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),

    # We receive our own join
    expect_unordered(
        [
            "/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        [
            "/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"

        ]
    ),

    # A different user joins the same room
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
    expect_unordered(
        # The new user’s presence is sent to the the existing occupant (two resources)
        [
            "/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']"
        ],
        [
            "/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_two}']"
        ],
        # the new user receives her own presence
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        # the new user receives the presence of the existing occupant
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']",
        ],
        [
            "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]",
        ],
    ),

    # That second user sends a private message to the first one
    send_stanza("<message from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' type='chat'><body>RELLO</body></message>"),

    # Message is received with a server-wide JID, by the two resources behind nick_one
    expect_unordered(
        [
            "/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='RELLO']",
            "/message/hints:no-copy",
            "/message/carbon:private",
            "!/message/muc_user:x",
        ],
        [
            "/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_two}'][@type='chat']/body[text()='RELLO']",
            "/message/hints:no-copy",
            "/message/carbon:private",
            "!/message/muc_user:x",
        ]
    ),

    # First occupant (with the two resources) changes her/his nick to a conflicting one
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
    expect_unordered(
        ["/message[@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='irc.localhost: Bobby: Nickname is already in use.']"],
        ["/message[@to='{jid_one}/{resource_two}'][@type='chat']/body[text()='irc.localhost: Bobby: Nickname is already in use.']"],
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}'][@type='error']"],
        ["/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_two}'][@type='error']"]
    ),

    # First occupant (with the two resources) changes her/his nick
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_three}' />"),
    expect_unordered(
        [
            "/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_two}/{resource_one}'][@type='unavailable']/muc_user:x/muc_user:item[@nick='Bernard']",
            "/presence/muc_user:x/muc_user:status[@code='303']"
        ],
        [
            "/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_two}/{resource_one}']"
        ],
        [
            "/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='unavailable']/muc_user:x/muc_user:item[@nick='Bernard']",
            "/presence/muc_user:x/muc_user:status[@code='303']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        [
            "/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_one}/{resource_one}']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        [
            "/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_two}'][@type='unavailable']/muc_user:x/muc_user:item[@nick='Bernard']",
            "/presence/muc_user:x/muc_user:status[@code='303']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        [
            "/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_one}/{resource_two}']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ]
    ),

    # One resource leaves the server entirely.
    send_stanza("<presence type='unavailable' from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
    # The leave is forwarded only to that resource
    expect_stanza("/presence[@type='unavailable']/muc_user:x/muc_user:status[@code='110']",
                  "/presence/status[text()='Biboumi note: 1 resources are still in this channel.']",
    ),

    # The second user sends two new private messages to the first user
    send_stanza("<message from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_three}' type='chat'><body>first</body></message>"),
    send_stanza("<message from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_three}' type='chat'><body>second</body></message>"),

    # The first user receives the two messages, on the connected resource, once each
    expect_unordered(
        ["/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='first']"],
        ["/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='second']"]
    ),
)
