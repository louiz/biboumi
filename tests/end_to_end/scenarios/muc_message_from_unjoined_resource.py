from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send a channel message
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
    # Receive the message
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']",
                  "/message/stable_id:stanza-id[@by='#foo%{irc_server_one}'][@id]"),
    # Send a message from a resource that is not joined
    send_stanza("<message from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
    expect_stanza("/message[@type='error']/error[@type='modify']/stanza:text[text()='You are not a participant in this room.']",
                  "/message/error/stanza:not-acceptable"
    ),
)
