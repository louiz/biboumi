from scenarios import *

import scenarios.simple_channel_join

scenario = (
    # Join the channel with user 1
    scenarios.simple_channel_join.scenario,

    # Make it persistent for user 1
    send_stanza("<iq from='{jid_one}/{resource_one}' id='conf1' to='#foo%{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/muc#owner'/></iq>"),
    expect_stanza("/iq[@type='result']/muc_owner:query/dataform:x/dataform:field[@var='persistent'][@type='boolean']/dataform:value[text()='false']"),
    send_stanza("<iq from='{jid_one}/{resource_one}' id='conf2' to='#foo%{irc_server_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#owner'><x type='submit' xmlns='jabber:x:data'><field var='persistent' xmlns='jabber:x:data'><value>true</value></field></x></query></iq>"),
    expect_stanza("/iq[@type='result']"),

    # Check that the value is now effectively true
    send_stanza("<iq from='{jid_one}/{resource_one}' id='conf1' to='#foo%{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/muc#owner'/></iq>"),
    expect_stanza("/iq[@type='result']/muc_owner:query/dataform:x/dataform:field[@var='persistent'][@type='boolean']/dataform:value[text()='true']"),

    # A second user joins the same channel
    send_stanza("<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_two}/{resource_one}'),
    expect_unordered(
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']"],
        [
            "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']",
            "/presence/muc_user:x/muc_user:status[@code='110']"
        ],
        ["/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']"],
        ["/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"]
    ),

    # First user leaves the room (but biboumi will stay in the channel)
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' type='unavailable' />"),

    # Only user 1 receives the unavailable presence
    expect_stanza("/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='unavailable']/muc_user:x/muc_user:status[@code='110']",
                  "/presence/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"),

    # Second user sends a channel message
    send_stanza("<message type='groupchat' from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}'><body>coucou</body></message>"),

    # Message should only be received by user 2, since user 1 has no resource in the room
    expect_stanza("/message[@type='groupchat'][@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']"),

    # Second user leaves the channel
    send_stanza("<presence type='unavailable' from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
    expect_stanza("/presence[@type='unavailable'][@from='#foo%{irc_server_one}/{nick_two}']"),
)
