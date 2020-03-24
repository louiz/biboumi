from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,

    # Send one channel message
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']"),

    # Second user joins
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='{lower_nick_one}%{irc_server_one}/~{nick_one}@localhost.localdomain'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    # Receive the history message
    expect_stanza("/message[@from='#foo%{irc_server_one}/{nick_one}']/body[text()='coucou']",
                  "/message/delay:delay[@from='#foo%{irc_server_one}']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
)
