from scenarios import *

conf = 'fixed_server'

scenario = (
    scenarios.channel_join_on_fixed_irc_server.scenario,

    # Send one channel message
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' type='groupchat'><body>coucou</body></message>"),
    expect_stanza("/message[@from='#foo@{biboumi_host}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']"),

    # Second user joins
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo@{biboumi_host}/{nick_one}' />"),
    expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo@{biboumi_host}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='{lower_nick_one}@{biboumi_host}/~{nick_one}@localhost'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    # Receive the history message
    expect_stanza("/message[@from='#foo@{biboumi_host}/{nick_one}']/body[text()='coucou']",
                  "/message/delay:delay[@from='#foo@{biboumi_host}']"),
    expect_stanza("/message[@from='#foo@{biboumi_host}'][@type='groupchat']/subject[not(text())]"),
)
