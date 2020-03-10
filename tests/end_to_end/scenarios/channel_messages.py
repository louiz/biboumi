from scenarios import *

import scenarios.simple_channel_join

scenario = (
    scenarios.simple_channel_join.scenario,

    # Second user joins
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),

    # Our presence, sent to the other user, and ourself
    expect_unordered(
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost'][@role='participant']"],
        ["/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost'][@role='participant']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        ["/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"],
    ),

    # Send a channel message
    send_stanza("<message id='first_id' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
    # Receive the message, forwarded to the two users
    expect_unordered(
        [
            "/message[@id='first_id'][@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']",
            "/message/stable_id:stanza-id[@by='#foo%{irc_server_one}'][@id]"
        ],
        [
            "/message[@id][@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_two}/{resource_one}'][@type='groupchat']/body[text()='coucou']",
            "/message/stable_id:stanza-id[@by='#foo%{irc_server_one}'][@id]"
        ]
    ),

    # Send a private message, to a in-room JID
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' type='chat'><body>coucou in private</body></message>"),
    # Message is received with a server-wide JID
    expect_stanza("/message[@from='{lower_nick_one}%{irc_server_one}'][@to='{jid_two}/{resource_one}'][@type='chat']/body[text()='coucou in private']"),
    # Respond to the message, to the server-wide JID
    send_stanza("<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>yes</body></message>"),
    # The response is received from the in-room JID
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_two}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='yes']",
                  "/message/muc_user:x"),
    # Do the exact same thing, from a different chan,
    # to check if the response comes from the right JID
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#dummy%{irc_server_one}/{nick_one}' />"),
    expect_stanza("/message"),
    expect_stanza("/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#dummy%{irc_server_one}'][@type='groupchat']/subject"),
    # Send a private message, to a in-room JID
    send_stanza("<message from='{jid_one}/{resource_one}' to='#dummy%{irc_server_one}/{nick_two}' type='chat'><body>re in private</body></message>"),

    # Message is received with a server-wide JID
    expect_stanza("/message[@from='{lower_nick_one}%{irc_server_one}'][@to='{jid_two}/{resource_one}'][@type='chat']/body[text()='re in private']"),

    # Respond to the message, to the server-wide JID
    send_stanza("<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>re</body></message>"),
    # The response is received from the in-room JID
    expect_stanza("/message[@from='#dummy%{irc_server_one}/{nick_two}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='re']"),

    # Now we leave the room, to check if the subsequent private messages are still received properly
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#dummy%{irc_server_one}/{nick_one}' type='unavailable' />"),
    expect_stanza("/presence[@type='unavailable']/muc_user:x/muc_user:status[@code='110']"),

    # The private messages from this nick should now come (again) from the server-wide JID
    send_stanza("<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>hihihoho</body></message>"),
    expect_stanza("/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}']"),
)
