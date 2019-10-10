from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,
    # Second resource, same channel
    send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
    expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),
    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

    # Now the first resource has an error
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%%{irc_server_one}/{nick_one}' type='error'><error type='cancel'><recipient-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></message>"),
    # Receive a leave only to the leaving resource
    expect_stanza("/presence[@type='unavailable'][@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}']/muc_user:x/muc_user:status[@code='110']",
                  "/presence/status[text()='Biboumi note: 1 resources are still in this channel.']"),
)
