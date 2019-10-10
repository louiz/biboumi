from scenarios import *

scenario = (
    scenarios.channel_join_with_two_users.scenario,
    # Send a channel message
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>/me rit en IRC</body></message>"),
    # Receive the message, forwarded to the two users
    expect_unordered(
        [
            "/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='/me rit en IRC']",
            "/message/stable_id:stanza-id[@by='#foo%{irc_server_one}'][@id]"
        ],
        [
            "/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_two}/{resource_one}'][@type='groupchat']/body[text()='/me rit en IRC']",
            "/message/stable_id:stanza-id[@by='#foo%{irc_server_one}'][@id]"
        ],
    ),
)
