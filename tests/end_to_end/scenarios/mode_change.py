from scenarios import *

scenario = (
    scenarios.channel_join_with_two_users.scenario,

    # Change a user mode with a message starting with /mode
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>/mode +v {nick_two}</body></message>"),
    expect_unordered(
        ["/message[@to='{jid_one}/{resource_one}']/body[text()='Mode #foo [+v {nick_two}] by {nick_one}']"],
        ["/message[@to='{jid_two}/{resource_one}']/body[text()='Mode #foo [+v {nick_two}] by {nick_one}']"],
        ["/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='member'][@role='participant']"],
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='member'][@role='participant']"],
    ),

    # using an iq
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='#foo%{irc_server_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item affiliation='admin' nick='{nick_two}'/></query></iq>"),
    expect_unordered(
        ["/message[@to='{jid_one}/{resource_one}']/body[text()='Mode #foo [+o {nick_two}] by {nick_one}']"],
        ["/message[@to='{jid_two}/{resource_one}']/body[text()='Mode #foo [+o {nick_two}] by {nick_one}']"],
        ["/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
        ["/iq[@id='id1'][@type='result'][@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}']"],
    ),

    # Remove +v manually. User ONLY has +o now. This doesn’t change the role/affiliation
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>/mode -v {nick_two}</body></message>"),
    expect_unordered(
        ["/message[@to='{jid_one}/{resource_one}']/body[text()='Mode #foo [-v {nick_two}] by {nick_one}']"],
        ["/message[@to='{jid_two}/{resource_one}']/body[text()='Mode #foo [-v {nick_two}] by {nick_one}']"],
        ["/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"],
    ),


    # remove the mode
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='#foo%{irc_server_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item affiliation='member' nick='{nick_two}' role='participant'/></query></iq>"),
    expect_unordered(
        ["/message[@to='{jid_one}/{resource_one}']/body[text()='Mode #foo [+v-o {nick_two} {nick_two}] by {nick_one}']"],
        ["/message[@to='{jid_two}/{resource_one}']/body[text()='Mode #foo [+v-o {nick_two} {nick_two}] by {nick_one}']"],
        ["/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='member'][@role='participant']"],
        ["/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='member'][@role='participant']"],
        ["/iq[@id='id1'][@type='result'][@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}']"],
    ),

    # using an iq, an a non-existant nick
    send_stanza("<iq from='{jid_one}/{resource_one}' id='id1' to='#foo%{irc_server_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item affiliation='admin' nick='blectre'/></query></iq>"),
    expect_stanza("/iq[@type='error']"),

    # using an iq, without the rights to do it
    send_stanza("<iq from='{jid_two}/{resource_one}' id='id1' to='#foo%{irc_server_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item affiliation='admin' nick='{nick_one}'/></query></iq>"),
    expect_unordered(
        ["/iq[@type='error']"],
        ["/message[@type='chat'][@to='{jid_two}/{resource_one}']"]
    ),

    # using an iq, with an unknown mode
    send_stanza("<iq from='{jid_two}/{resource_one}' id='id1' to='#foo%{irc_server_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item affiliation='owner' nick='{nick_one}'/></query></iq>"),
    expect_unordered(
        ["/iq[@type='error']"],
        ["/message[@type='chat'][@to='{jid_two}/{resource_one}']"],
    ),
)
