from scenarios import *

import scenarios.simple_channel_join

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send a multi-line channel message
    send_stanza("<message id='the-message-id' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>un\ndeux\ntrois</body></message>"),
    # Receive multiple messages, in order
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@id='the-message-id'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='un']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@id][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='deux']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@id][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='trois']"),

    # Send a simple message, with no id
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>hello</body></message>"),

    # Expect a non-empty id as a result (should be a uuid)
    expect_stanza("!/message[@id='']",
                  "/message[@id]/body[text()='hello']"),

    # even though we reflect the message to XMPP only
    # when we send it to IRC, there’s still a race
    # condition if the XMPP client receives the
    # reflection (and the IRC server didn’t yet receive
    # it), then the new user joins the room, and then
    # finally the IRC server sends the message to “all
    # participants of the channel”, including the new
    # one, that was not supposed to be there when the
    # message was sent in the first place by the first
    # XMPP user. There’s nothing we can do about it until
    # all servers support the echo-message IRCv3
    # extension… So, we just sleep a little bit before
    # joining the room with the new user.
    sleep_for(0.2),
    # Second user joins
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
    # Our presence, sent to the other user
    expect_unordered(
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost'][@role='participant']"],
        ["/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost'][@role='participant']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        ["/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"]
    ),

    # Send a multi-line channel message
    send_stanza("<message id='the-message-id' from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>a\nb\nc</body></message>"),
    # Receive multiple messages, for each user
    expect_unordered(
        ["/message[@from='#foo%{irc_server_one}/{nick_one}'][@id='the-message-id'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='a']"],
        ["/message[@from='#foo%{irc_server_one}/{nick_one}'][@id][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='b']"],
        ["/message[@from='#foo%{irc_server_one}/{nick_one}'][@id][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='c']"],

        ["/message[@from='#foo%{irc_server_one}/{nick_one}'][@id][@to='{jid_two}/{resource_one}'][@type='groupchat']/body[text()='a']"],
        ["/message[@from='#foo%{irc_server_one}/{nick_one}'][@id][@to='{jid_two}/{resource_one}'][@type='groupchat']/body[text()='b']"],
        ["/message[@from='#foo%{irc_server_one}/{nick_one}'][@id][@to='{jid_two}/{resource_one}'][@type='groupchat']/body[text()='c']"],
    )
)
