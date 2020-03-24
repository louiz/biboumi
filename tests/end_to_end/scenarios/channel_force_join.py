from scenarios import *

import scenarios.channel_join_with_two_users

scenario = (
    scenarios.channel_join_with_two_users.scenario,
    # Here we simulate a desynchronization of a client: The client thinks it’s
    # disconnected from the room, but biboumi still thinks it’s in the room. The
    # client thus sends a join presence, and biboumi should send everything
    # (user list, history, etc) in response.
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_three}'><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_unordered(
             [
                  "/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost.localdomain']"
             ],
             [
                  "/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"
             ],
             [
                  "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"
             ]
    ),
)

