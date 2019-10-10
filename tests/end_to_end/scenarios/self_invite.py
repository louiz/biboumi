from scenarios import *

scenario = (
    scenarios.simple_channel_join.scenario,
    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><x xmlns='http://jabber.org/protocol/muc#user'><invite to='{nick_one}'/></x></message>"),
    expect_stanza("/message/body[text()='{nick_one} is already on channel #foo']")
)
