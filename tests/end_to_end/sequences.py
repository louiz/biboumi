from functions import expect_stanza, send_stanza, common_replacements

def handshake():
    return (
            expect_stanza("//handshake"),
            send_stanza("<handshake xmlns='jabber:component:accept'/>")
           )

def connection_begin(irc_host, jid, expected_irc_presence=False, fixed_irc_server=False):
    jid = jid.format_map(common_replacements)
    if fixed_irc_server:
        xpath    = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[text()='%s']"
        xpath_re = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[re:test(text(), '%s')]"
    else:
        xpath    = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[text()='%s']"
        xpath_re = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[re:test(text(), '%s')]"
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

    # These five messages can be receive in any order
    result += (
    expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % 'irc.localhost')),
    expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % 'irc.localhost')),
    expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % 'irc.localhost')),
    expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % 'irc.localhost')),
    expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % 'irc.localhost')),
    )

    return result

def connection_tls_begin(irc_host, jid, fixed_irc_server):
    jid = jid.format_map(common_replacements)
    if fixed_irc_server:
        xpath    = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[text()='%s']"
        xpath_re = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[re:test(text(), '%s')]"
    else:
        xpath    = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[text()='%s']"
        xpath_re = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[re:test(text(), '%s')]"
    irc_host = 'irc.localhost'
    return (
        expect_stanza(
                (xpath % ('Connecting to %s:7778 (encrypted)' % irc_host),
                 "/message/hints:no-copy",
                 "/message/carbon:private",
                )
               ),
        expect_stanza(xpath % 'Connected to IRC server (encrypted).'),
        # These five messages can be receive in any order
        expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % irc_host)),
        expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % irc_host)),
        expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % irc_host)),
        expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % irc_host)),
        expect_stanza(xpath_re % (r'^%s: (\*\*\* Checking Ident|\*\*\* Looking up your hostname\.\.\.|\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* Got Ident response)$' % irc_host)),
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
    expect_stanza(xpath_re % (r'^%s: Your host is .*$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: This server was created .*$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: There are \d+ users and \d+ invisible on \d+ servers$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: \d+ unknown connection\(s\)$' % irc_host), optional=True),
    expect_stanza(xpath_re % (r'^%s: \d+ channels formed$' % irc_host), optional=True),
    expect_stanza(xpath_re % (r'^%s: I have \d+ clients and \d+ servers$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: \d+ \d+ Current local users \d+, max \d+$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: \d+ \d+ Current global users \d+, max \d+$' % irc_host)),
    expect_stanza(xpath_re % (r'^%s: Highest connection count: \d+ \(\d+ clients\) \(\d+ connections received\)$' % irc_host)),
    expect_stanza(xpath % "- This is charybdis MOTD you might replace it, but if not your friends will\n- laugh at you.\n"),
    expect_stanza(xpath_re % r'^User mode for \w+ is \[\+Z?i\]$'),
    )

def connection_middle(irc_host, jid, fixed_irc_server=False):
    if fixed_irc_server:
        xpath_re = "/message[@to='" + jid + "'][@from='biboumi.localhost']/body[re:test(text(), '%s')]"
    else:
        xpath_re = "/message[@to='" + jid + "'][@from='" + irc_host + "@biboumi.localhost']/body[re:test(text(), '%s')]"
    irc_host = 'irc.localhost'
    return (
        expect_stanza(xpath_re % (r'^%s: \*\*\* You are exempt from flood limits$' % irc_host)),
    )


def connection(irc_host="irc.localhost", jid="{jid_one}/{resource_one}", expected_irc_presence=False, fixed_irc_server=False):
    return connection_begin(irc_host, jid, expected_irc_presence, fixed_irc_server=fixed_irc_server) + \
           connection_middle(irc_host, jid, fixed_irc_server=fixed_irc_server) + \
           connection_end(irc_host, jid, fixed_irc_server=fixed_irc_server)

def connection_tls(irc_host="irc.localhost", jid="{jid_one}/{resource_one}", fixed_irc_server=False):
    return connection_tls_begin(irc_host, jid, fixed_irc_server) + \
           connection_middle(irc_host, jid, fixed_irc_server) +\
           connection_end(irc_host, jid, fixed_irc_server)

