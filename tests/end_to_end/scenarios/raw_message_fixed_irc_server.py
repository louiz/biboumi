from scenarios import *

conf = 'fixed_server'

scenario = (
    sequences.handshake(),

    send_stanza("<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
    sequences.connection("irc.localhost", '{jid_one}/{resource_one}', fixed_irc_server=True),
    expect_stanza("/message"),
    expect_stanza("/presence"),
    expect_stanza("/message"),
    
    send_stanza("<message from='{jid_one}/{resource_one}' to='{biboumi_host}' type='chat'><body>WHOIS {nick_one}</body></message>"),
    expect_stanza("/message[@from='{biboumi_host}'][@type='chat']/body[text()='irc.localhost: {nick_one} ~{nick_one} localhost * {nick_one}']"),
    
)
