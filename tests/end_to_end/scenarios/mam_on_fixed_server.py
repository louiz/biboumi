from scenarios import *

conf = 'fixed_server'

scenario = (
    scenarios.channel_join_on_fixed_irc_server.scenario,
    
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' type='groupchat'><body>coucou</body></message>"),
    expect_stanza("/message[@from='#foo@{biboumi_host}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']"),

    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' type='groupchat'><body>coucou 2</body></message>"),
    expect_stanza("/message[@from='#foo@{biboumi_host}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou 2']"),

    # Retrieve the complete archive
    send_stanza("<iq to='#foo@{biboumi_host}' from='{jid_one}/{resource_one}' type='set' id='id1'><query xmlns='urn:xmpp:mam:2' queryid='qid1' /></iq>"),

    expect_stanza("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo@{biboumi_host}/{nick_one}'][@type='groupchat']/client:body[text()='coucou']"),
    expect_stanza("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                  "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo@{biboumi_host}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 2']"),
)
