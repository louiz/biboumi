import sequences

import scenarios.channel_join_with_two_users

from functions import send_stanza, expect_stanza, expect_unordered

scenario = (
    scenarios.channel_join_with_two_users.scenario,
    # Here we simulate a desynchronization of a client: The client thinks it’s
    # disconnected from the room, but biboumi still thinks it’s in the room. The
    # client thus sends a join presence, and biboumi should send everything
    # (user list, history, etc) in response.
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_three}'><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    expect_unordered(
             [
                  "/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant'][@jid='{lower_nick_two}%{irc_server_one}/~{nick_two}@localhost']"
             ],
             [
                  "/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"
             ],
             [
                  "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"
             ]
    ),

    # And also, that was not the same nickname, so everyone receives a nick change
    expect_unordered(
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_two}/{resource_one}'][@type='unavailable']/muc_user:x/muc_user:item[@nick='Bernard']",
                  "/presence/muc_user:x/muc_user:status[@code='303']",
             ],
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_two}/{resource_one}']",
             ],
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='unavailable']/muc_user:x/muc_user:item[@nick='Bernard']",
                  "/presence/muc_user:x/muc_user:status[@code='303']",
                  "/presence/muc_user:x/muc_user:status[@code='110']",
             ],
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_one}/{resource_one}']",
                  "/presence/muc_user:x/muc_user:status[@code='110']",
             ],
    ),
)
