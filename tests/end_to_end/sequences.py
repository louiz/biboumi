from functions import expect_stanza, send_stanza, common_replacements, expect_unordered

def handshake():
    return (
            expect_stanza("//handshake"),
            send_stanza("<handshake xmlns='jabber:component:accept'/>")
           )

def connection_begin(irc_host, jid, expected_irc_presence=False, fixed_irc_server=False, login=None):
    jid = jid.format_map(common_replacements)
    if fixed_irc_server:
        xpath    = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[text()='%s']"
    else:
        xpath    = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[text()='%s']"
    result = (
    expect_stanza(xpath % ('Connecting to %s:6697 (encrypted)' % irc_host),
                  "/message/hints:no-copy",
                  "/message/carbon:private"
                 ),
    expect_stanza(xpath % 'Connection failed: Connection refused'),
    expect_stanza(xpath % ('Connecting to %s:6670 (encrypted)' % irc_host)),
    expect_stanza(xpath % 'Connection failed: Connection refused'),
    expect_stanza(xpath % ('Connecting to %s:6667 (not encrypted)' % irc_host)),
    expect_stanza(xpath % 'Connected to IRC server.'))

    if expected_irc_presence:
        result += (expect_stanza("/presence[@from='" + irc_host + "@biboumi.localhost']"),)

    if login is not None:
        result += (expect_stanza("/message/body[text()='irc.localhost: You are now logged in as %s']" % (login,)),)
    result += (
    expect_stanza("/message/body[text()='irc.localhost: *** Looking up your hostname...']"),
    expect_stanza("/message/body[text()='irc.localhost: *** Found your hostname']")
    )
    return result

def connection_tls_begin(irc_host, jid, fixed_irc_server):
    jid = jid.format_map(common_replacements)
    if fixed_irc_server:
        xpath    = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[text()='%s']"
    else:
        xpath    = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[text()='%s']"
    irc_host = 'irc.localhost'
    return (
        expect_stanza(xpath % ('Connecting to %s:7778 (encrypted)' % irc_host),
                      "/message/hints:no-copy",
                      "/message/carbon:private",
               ),
        expect_stanza(xpath % 'Connected to IRC server (encrypted).'),
        expect_stanza("/message/body[text()='irc.localhost: *** Looking up your hostname...']"),
        expect_stanza("/message/body[text()='irc.localhost: *** Found your hostname']")
    )

def connection_end(irc_host, jid, fixed_irc_server=False):
    jid = jid.format_map(common_replacements)
    if fixed_irc_server:
        xpath    = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[text()='%s']"
        xpath_re = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[re:test(text(), '%s')]"
    else:
        xpath    = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[text()='%s']"
        xpath_re = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[re:test(text(), '%s')]"
    irc_host = 'irc.localhost'
    return (
    expect_stanza("/message/body[re:test(text(), '%s')]" % (r'^%s: Your host is %s, running version oragono-2\.0\.0(-[a-z0-9]+)? $' % (irc_host, irc_host))),
    expect_stanza(xpath_re % (r'^%s: This server was created .*$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: There are \d+ users and \d+ invisible on \d+ server\(s\)$' % irc_host)),
    expect_stanza(xpath_re % ("%s: \d+ IRC Operators online" % irc_host,)),
    expect_stanza(xpath_re % ("%s: \d+ unregistered connections" % irc_host,)),
    expect_stanza(xpath_re % ("%s: \d+ channels formed" % irc_host,)),
    expect_stanza(xpath_re % (r'^%s: I have \d+ clients and \d+ servers$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: \d+ \d+ Current local users \d+, max \d+$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: \d+ \d+ Current global users \d+, max \d+$' % irc_host)),
    expect_stanza(xpath % "%s: MOTD File is missing: Unspecified error" % irc_host),
    expect_stanza(xpath_re % (r'.+? \+Z',)),
    )


def connection(irc_host="irc.localhost", jid="{jid_one}/{resource_one}", expected_irc_presence=False, fixed_irc_server=False, login=None):
    return connection_begin(irc_host, jid, expected_irc_presence, fixed_irc_server=fixed_irc_server, login=login) + \
           connection_end(irc_host, jid, fixed_irc_server=fixed_irc_server)

def connection_tls(irc_host="irc.localhost", jid="{jid_one}/{resource_one}", fixed_irc_server=False):
    return connection_tls_begin(irc_host, jid, fixed_irc_server) + \
           connection_end(irc_host, jid, fixed_irc_server)

