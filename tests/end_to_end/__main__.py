#!/usr/bin/env python3

import collections
import slixmpp
import asyncio
import logging
import signal
import atexit
import lxml.etree
import sys
import io
import os
from functools import partial
from slixmpp.xmlstream.matcher.base import MatcherBase


class MatchAll(MatcherBase):
    """match everything"""

    def match(self, xml):
        return True


class StanzaError(Exception):
    """
    Raised when a step fails.
    """
    pass


class SkipStepError(Exception):
    """
    Raised by a step when it needs to be skiped, by running
    the next available step immediately.
    """
    pass


class XMPPComponent(slixmpp.BaseXMPP):
    """
    XMPPComponent sending a “scenario” of stanzas, checking that the responses
    match the expected results.
    """

    def __init__(self, scenario, biboumi):
        super().__init__(jid="biboumi.localhost", default_ns="jabber:component:accept")
        self.is_component = True
        self.stream_header = '<stream:stream %s %s from="%s" id="%s">' % (
            'xmlns="jabber:component:accept"',
            'xmlns:stream="%s"' % self.stream_ns,
            self.boundjid, self.get_id())
        self.stream_footer = "</stream:stream>"

        self.register_handler(slixmpp.Callback('Match All',
                                               MatchAll(None),
                                               self.handle_incoming_stanza))

        self.add_event_handler("session_end", self.on_end_session)

        asyncio.async(self.accept_routine())

        self.scenario = scenario
        self.biboumi = biboumi
        # A callable, taking a stanza as argument and raising a StanzaError
        # exception if the test should fail.
        self.stanza_checker = None
        self.failed = False
        self.accepting_server = None

    def error(self, message):
        print("[31;1mFailure[0m: %s" % (message,))
        self.scenario.steps = []
        self.failed = True

    def on_end_session(self, event):
        self.loop.stop()

    def handle_incoming_stanza(self, stanza):
        if self.stanza_checker:
            try:
                self.stanza_checker(stanza)
            except StanzaError as e:
                self.error(e)
            except SkipStepError:
                # Run the next step and then re-handle this same stanza
                self.run_scenario()
                return self.handle_incoming_stanza(stanza)
            self.stanza_checker = None
        self.run_scenario()

    def run_scenario(self):
        if scenario.steps:
            step = scenario.steps.pop(0)
            step(self, self.biboumi)
        else:
            self.biboumi.stop()

    @asyncio.coroutine
    def accept_routine(self):
        self.accepting_server = yield from self.loop.create_server(lambda: self,
                                                                   "127.0.0.1", "8811", reuse_address=True)

    def check_stanza_against_all_expected_xpaths(self):
        pass


def check_xpath(xpaths, stanza):
    for xpath in xpaths:
        tree = lxml.etree.parse(io.StringIO(str(stanza)))
        matched = tree.xpath(xpath, namespaces={'re': 'http://exslt.org/regular-expressions',
                                                'muc_user': 'http://jabber.org/protocol/muc#user'})
        if not matched:
            raise StanzaError("Received stanza “%s” did not match expected xpath “%s”" % (stanza, xpath))


def check_xpath_optional(xpaths, stanza):
    try:
        check_xpath(xpaths, stanza)
    except StanzaError:
        raise SkipStepError()


class Scenario:
    """Defines a list of actions that are executed in sequence, until one of
    them throws an exception, or until the end.  An action can be something
    like “send a stanza”, “receive the next stanza and check that it matches
    the given XPath”, “send a signal”, “wait for the end of the process”,
    etc
    """

    def __init__(self, name, steps, conf="basic"):
        """
        Steps is a list of 2-tuple:
        [(action, answer), (action, answer)]
        """
        self.name = name
        self.steps = []
        self.conf = conf
        for elem in steps:
            if isinstance(elem, collections.Iterable):
                for step in elem:
                    self.steps.append(step)
            else:
                self.steps.append(elem)


class ProcessRunner:
    def __init__(self):
        self.process = None
        self.signal_sent = False

    @asyncio.coroutine
    def start(self):
        self.process = yield from self.create

    @asyncio.coroutine
    def wait(self):
        code = yield from self.process.wait()
        return code

    def stop(self):
        if not self.signal_sent:
            self.signal_sent = True
            if self.process:
                self.process.send_signal(signal.SIGINT)

    def __del__(self):
        self.stop()


class BiboumiRunner(ProcessRunner):
    def __init__(self, name, with_valgrind):
        super().__init__()
        self.name = name
        self.fd = open("biboumi_%s_output.txt" % (name,), "w")
        if with_valgrind:
            self.create = asyncio.create_subprocess_exec("valgrind", "--leak-check=full", "--show-leak-kinds=all",
                                                         "--errors-for-leak-kinds=all", "--error-exitcode=16",
                                                         "./biboumi", "test.conf", stdin=None, stdout=self.fd,
                                                         stderr=self.fd, loop=None, limit=None)
        else:
            self.create = asyncio.create_subprocess_exec("./biboumi", "test.conf", stdin=None, stdout=self.fd,
                                                         stderr=self.fd, loop=None, limit=None)


class IrcServerRunner(ProcessRunner):
    def __init__(self):
        super().__init__()
        self.create = asyncio.create_subprocess_exec("charybdis", "-foreground", "-configfile", os.getcwd() + "/../tests/end_to_end/ircd.conf",
                                                     stderr=asyncio.subprocess.PIPE)


def send_stanza(stanza, xmpp, biboumi):
    xmpp.send_raw(stanza.format_map(common_replacements))
    asyncio.get_event_loop().call_soon(xmpp.run_scenario)


def expect_stanza(xpaths, xmpp, biboumi, optional=False):
    check_func = check_xpath if not optional else check_xpath_optional
    if isinstance(xpaths, str):
        xmpp.stanza_checker = partial(check_func, [xpaths.format_map(common_replacements)])
    elif isinstance(xpaths, tuple):
        xmpp.stanza_checker = partial(check_func, [xpath.format_map(common_replacements) for xpath in xpaths])
    else:
        print("Warning, from argument type passed to expect_stanza: %s" % (type(xpaths)))


class BiboumiTest:
    """
    Spawns a biboumi process and a fake XMPP Component that will run a
    Scenario.  It redirects the outputs of the subprocess into separated
    files, and detects any failure in the running of the scenario.
    """

    def __init__(self, scenario, expected_code=0):
        self.scenario = scenario
        self.expected_code = 0

    def run(self, with_valgrind=True):
        print("Running scenario: [33;1m%s[0m%s" % (self.scenario.name, " (with valgrind)" if with_valgrind else ''))
        # Redirect the slixmpp logging into a specific file
        output_filename = "slixmpp_%s_output.txt" % (self.scenario.name,)
        with open(output_filename, "w"):
            pass
        logging.basicConfig(level=logging.DEBUG,
                            format='%(levelname)-8s %(message)s',
                            filename=output_filename)

        with open("test.conf", "w") as fd:
            fd.write(confs[scenario.conf])

        # Start the XMPP component and biboumi
        biboumi = BiboumiRunner(scenario.name, with_valgrind)
        xmpp = XMPPComponent(self.scenario, biboumi)
        asyncio.get_event_loop().run_until_complete(biboumi.start())

        asyncio.get_event_loop().call_soon(xmpp.run_scenario)

        xmpp.process()

        code = asyncio.get_event_loop().run_until_complete(biboumi.wait())
        failed = False
        if not xmpp.failed:
            if code != self.expected_code:
                xmpp.error("Wrong return code from biboumi's process: %d" % (code,))
                failed = True
            else:
                print("[32;1mSuccess![0m")
        else:
            failed = True

        if xmpp.server:
            xmpp.accepting_server.close()

        return not failed


confs = {
'basic':
"""hostname=biboumi.localhost
password=coucou
db_name=biboumi.sqlite
port=8811"""}

common_replacements = {
    'irc_server_one': 'irc.localhost@biboumi.localhost',
    'irc_host_one': 'irc.localhost',
    'resource_one': 'resource1',
    'nick_one': 'Nick',
    'jid_one': 'first@example.com',
    'jid_two': 'second@example.com',
    'nick_two': 'Bobby',
}


def handshake_sequence():
    return (partial(expect_stanza, "//handshake"),
            partial(send_stanza, "<handshake xmlns='jabber:component:accept'/>"))


def connection_sequence(irc_host, jid):
    jid = jid.format_map(common_replacements)
    xpath    = "/message[@to='" + jid + "'][@from='irc.localhost@biboumi.localhost']/body[text()='%s']"
    xpath_re = "/message[@to='" + jid + "'][@from='irc.localhost@biboumi.localhost']/body[re:test(text(), '%s')]"
    return (
    partial(expect_stanza,
          xpath % ('Connecting to %s:6697 (encrypted)' % irc_host)),
    partial(expect_stanza,
            xpath % 'Connection failed: Connection refused'),
    partial(expect_stanza,
          xpath % ('Connecting to %s:6670 (encrypted)' % irc_host)),
    partial(expect_stanza,
            xpath % 'Connection failed: Connection refused'),
    partial(expect_stanza,
          xpath % ('Connecting to %s:6667 (not encrypted)' % irc_host)),
    partial(expect_stanza,
            xpath % 'Connected to IRC server.'),
    # These two messages can be receive in any order
    partial(expect_stanza,
          xpath_re % (r'^%s: \*\*\* (Checking Ident|Looking up your hostname...)$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: \*\*\* (Checking Ident|Looking up your hostname...)$' % irc_host)),
    # These three messages can be received in any order
    partial(expect_stanza,
          xpath_re % (r'^%s: (\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* No Ident response)$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: (\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* No Ident response)$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: (\*\*\* Found your hostname: .*|ACK multi-prefix|\*\*\* No Ident response)$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: Your host is .*$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: This server was created .*$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: There are \d+ users and \d+ invisible on \d+ servers$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: \d+ channels formed$' % irc_host), optional=True),
    partial(expect_stanza,
          xpath_re % (r'^%s: I have \d+ clients and \d+ servers$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: \d+ \d+ Current local users \d+, max \d+$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: \d+ \d+ Current global users \d+, max \d+$' % irc_host)),
    partial(expect_stanza,
          xpath_re % (r'^%s: Highest connection count: \d+ \(\d+ clients\) \(\d+ connections received\)$' % irc_host)),
    partial(expect_stanza,
            xpath % "- This is charybdis MOTD you might replace it, but if not your friends will\n- laugh at you.\n"),
    partial(expect_stanza,
            xpath_re % r'^User mode for \w+ is \[\+i\]$'),
    )


if __name__ == '__main__':

    atexit.register(asyncio.get_event_loop().close)

    # Start the test component, accepting connections on the configured
    # port.
    scenarios = (
        Scenario("basic_handshake_success",
                 [
                     handshake_sequence()
                 ]),
        Scenario("irc_server_connection",
                 [
                     handshake_sequence(),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                 ]),
        Scenario("simple_channel_join",
                 [
                     handshake_sequence(),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                             "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
                 ]),
        Scenario("channel_join_with_two_users",
                 [
                     handshake_sequence(),
                     # First user joins
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='~nick@localhost'][@role='moderator']",
                             "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

                     # Second user joins
                     partial(send_stanza,
                             "<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     connection_sequence("irc.localhost", '{jid_two}/{resource_one}'),
                     # Our presence, sent to the other user
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",)),
                     # The other user presence
                     partial(expect_stanza,
                             "/presence[@to='{jid_second}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~nick@localhost'][@role='participant']"),
                     # Our own presence
                     partial(expect_stanza,
                             ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
                 ]),
        Scenario("channel_custom_topic",
                 [
                     handshake_sequence(),
                     # First user joins
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='~nick@localhost'][@role='moderator']",
                             "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

                     # First user sets the topic
                     partial(send_stanza,
                             "<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><subject>TOPIC TEST</subject></message>"),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat'][@to='{jid_one}/{resource_one}']/subject[text()='TOPIC TEST']"),

                     # Second user joins
                     partial(send_stanza,
                             "<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     connection_sequence("irc.localhost", '{jid_two}/{resource_one}'),
                     # Our presence, sent to the other user
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",)),
                     # The other user presence
                     partial(expect_stanza,
                             "/presence[@to='{jid_second}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='~nick@localhost'][@role='moderator']"),
                     # Our own presence
                     partial(expect_stanza,
                             ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/subject[text()='TOPIC TEST']"),
                 ]),
    )

    failures = 0

    irc_output = open("irc_output.txt", "w")
    irc = IrcServerRunner()
    print("Starting irc server…")
    asyncio.get_event_loop().run_until_complete(irc.start())
    while True:
        res = asyncio.get_event_loop().run_until_complete(irc.process.stderr.readline())
        irc_output.write(res.decode())
        if not res:
            print("IRC server failed to start, see irc_output.txt for more details. Exiting…")
            sys.exit(1)
        if b"now running in foreground mode" in res:
            break
    print("irc server started.")
    print("Running %s checks for biboumi." % (len(scenarios)))

    for scenario in scenarios:
        test = BiboumiTest(scenario)
        if not test.run(False):
            print("You can check the files slixmpp_%s_output.txt and biboumi_%s_output.txt to help you debug." %
                  (scenario.name, scenario.name))
            failures += 1

    print("Waiting for irc server to exit…")
    irc.stop()
    code = asyncio.get_event_loop().run_until_complete(irc.wait())

    if failures:
        print("%d test%s failed, please fix %s." % (failures, 's' if failures > 1 else '',
                                                    'them' if failures > 1 else 'it'))
        sys.exit(1)
    else:
        print("All tests passed successfully")
