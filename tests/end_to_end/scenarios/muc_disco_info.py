from scenarios import *

scenario = (
    send_stanza("<iq from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' id='1' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='result']/disco_info:query",
                  "/iq[@type='result']/disco_info:query/disco_info:identity[@category='conference'][@type='irc'][@name='#foo on {irc_host_one}']",
                  "/iq/disco_info:query/disco_info:feature[@var='jabber:iq:version']",
                  "/iq/disco_info:query/disco_info:feature[@var='http://jabber.org/protocol/commands']",
                  "/iq/disco_info:query/disco_info:feature[@var='urn:xmpp:ping']",
                  "/iq/disco_info:query/disco_info:feature[@var='urn:xmpp:mam:2']",
                  "/iq/disco_info:query/disco_info:feature[@var='jabber:iq:version']",
                  "/iq/disco_info:query/disco_info:feature[@var='muc_nonanonymous']",
                  "/iq/disco_info:query/disco_info:feature[@var='urn:xmpp:sid:0']",
                  "!/iq/disco_info:query/dataform:x/dataform:field[@var='muc#roominfo_occupants']"),

    # Join the channel, and re-do the same query
    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' ><x xmlns='http://jabber.org/protocol/muc'/></presence>"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}'),
    expect_stanza("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                  "/presence/muc_user:x/muc_user:status[@code='110']"),

    expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

    send_stanza("<iq from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' id='2' type='get'><query xmlns='http://jabber.org/protocol/disco#info'/></iq>"),
    expect_stanza("/iq[@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='result']/disco_info:query",
                  "/iq/disco_info:query/dataform:x/dataform:field[@var='muc#roominfo_occupants']/dataform:value[text()='1']",
                  "/iq/disco_info:query/dataform:x/dataform:field[@var='FORM_TYPE'][@type='hidden']/dataform:value[text()='http://jabber.org/protocol/muc#roominfo']"),
)
