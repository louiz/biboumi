from scenarios import *

import scenarios.channel_join_with_two_users

scenario = (
    scenarios.channel_join_with_two_users.scenario,

    # first users changes their nick
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_three}' id='nick_change' />"),
    expect_unordered(
        ["/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_two}/{resource_one}'][@type='unavailable']",
         "/presence/muc_user:x/muc_user:status[@code='303']",
         "/presence/muc_user:x/muc_user:item[@affiliation='admin']",
         "/presence/muc_user:x/muc_user:item[@role='moderator']",
         "/presence/muc_user:x/muc_user:item[@nick='{nick_three}']"],

        ["/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='unavailable']",
         "/presence/muc_user:x/muc_user:status[@code='303']",
         "/presence/muc_user:x/muc_user:item[@nick='{nick_three}']",
         "/presence/muc_user:x/muc_user:item[@affiliation='admin']",
         "/presence/muc_user:x/muc_user:item[@role='moderator']",
         "/presence/muc_user:x/muc_user:status[@code='110']"],

        ["/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_two}/{resource_one}']",
         "/presence/muc_user:x/muc_user:item[@affiliation='admin']",
         "/presence/muc_user:x/muc_user:item[@role='moderator']"],

        ["/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_one}/{resource_one}']",
         "/presence/muc_user:x/muc_user:item[@affiliation='admin']",
         "/presence/muc_user:x/muc_user:item[@role='moderator']",
         "/presence/muc_user:x/muc_user:status[@code='110']"]
    ),
)
