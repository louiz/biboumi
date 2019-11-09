from scenarios import *

scenario = (
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%doesnotexist@{biboumi_host}/{nick_one}' />"),
    expect_stanza("/message/body[text()='Connecting to doesnotexist:6697 (encrypted)']"),
    expect_stanza("/message/body[re:test(text(), 'Connection failed: (Domain name not found|Name or service not known)')]"),
    expect_stanza("/presence[@from='#foo%doesnotexist@{biboumi_host}/{nick_one}']/muc:x",
                  "/presence/error[@type='cancel']/stanza:item-not-found",
                  "/presence/error[@type='cancel']/stanza:text[re:test(text(), '(Domain name not found|Name or service not known)')]",
                  ),
)
