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

        self.saved_values = {}

    def error(self, message):
        print("[31;1mFailure[0m: %s" % (message,))
        self.scenario.steps = []
        self.failed = True

    def on_end_session(self, _):
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
        if self.scenario.steps:
            step = self.scenario.steps.pop(0)
            step(self, self.biboumi)
        else:
            if self.biboumi:
                self.biboumi.stop()

    @asyncio.coroutine
    def accept_routine(self):
        self.accepting_server = yield from self.loop.create_server(lambda: self,
                                                                   "127.0.0.1", 8811, reuse_address=True)

    def check_stanza_against_all_expected_xpaths(self):
        pass


def match(stanza, xpath):
    tree = lxml.etree.parse(io.StringIO(str(stanza)))
    matched = tree.xpath(xpath, namespaces={'re': 'http://exslt.org/regular-expressions',
                                            'muc_user': 'http://jabber.org/protocol/muc#user',
                                            'disco_info': 'http://jabber.org/protocol/disco#info',
                                            'muc_traffic': 'http://jabber.org/protocol/muc#traffic',
                                            'disco_items': 'http://jabber.org/protocol/disco#items',
                                            'commands': 'http://jabber.org/protocol/commands',
                                            'dataform': 'jabber:x:data',
                                            'version': 'jabber:iq:version',
                                            'mam': 'urn:xmpp:mam:1',
                                            'delay': 'urn:xmpp:delay',
                                            'forward': 'urn:xmpp:forward:0',
                                            'client': 'jabber:client',
                                            'rsm': 'http://jabber.org/protocol/rsm',
                                            'carbon': 'urn:xmpp:carbons:2',
                                            'hints': 'urn:xmpp:hints'})
    return matched


def check_xpath(xpaths, xmpp, after, stanza):
    for i, xpath in enumerate(xpaths):
        matched = match(stanza, xpath)
        if not matched:
            raise StanzaError("Received stanza “%s” did not match expected xpath “%s”" % (stanza, xpath))
    if after:
        if isinstance(after, collections.Iterable):
            for af in after:
                af(stanza, xmpp)
        else:
            after(stanza, xmpp)

def all_xpaths_match(stanza, xpaths):
    for xpath in xpaths:
        matched = match(stanza, xpath)
        if not matched:
            return False
    return True

def check_list_of_xpath(list_of_xpaths, xmpp, stanza):
    found = None
    for i, xpaths in enumerate(list_of_xpaths):
        if all_xpaths_match(stanza, xpaths):
            found = i
            break

    if found is None:
        raise StanzaError("Received stanza “%s” did not match any of the expected xpaths:\n%s" % (stanza, list_of_xpaths))

    list_of_xpaths.pop(i)

    if list_of_xpaths:
        step = partial(expect_unordered_already_formatted, list_of_xpaths)
        xmpp.scenario.steps.insert(0, step)


def check_xpath_optional(xpaths, xmpp, after, stanza):
    try:
        check_xpath(xpaths, xmpp, after, stanza)
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
        self.create = None

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
    def __init__(self, name):
        super().__init__()
        self.name = name
        self.fd = open("biboumi_%s_output.txt" % (name,), "w")
        with_valgrind = os.environ.get("E2E_WITH_VALGRIND") is not None
        if with_valgrind:
            self.create = asyncio.create_subprocess_exec("valgrind", "--suppressions=" + (os.environ.get("E2E_BIBOUMI_SUPP_DIR") or "") + "biboumi.supp", "--leak-check=full", "--show-leak-kinds=all",
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
    replacements = common_replacements
    replacements.update(xmpp.saved_values)
    xmpp.send_raw(stanza.format_map(replacements))
    asyncio.get_event_loop().call_soon(xmpp.run_scenario)


def expect_stanza(xpaths, xmpp, biboumi, optional=False, after=None):
    check_func = check_xpath if not optional else check_xpath_optional
    if isinstance(xpaths, str):
        xmpp.stanza_checker = partial(check_func, [xpaths.format_map(common_replacements)], xmpp, after)
    elif isinstance(xpaths, tuple):
        xmpp.stanza_checker = partial(check_func, [xpath.format_map(common_replacements) for xpath in xpaths], xmpp, after)
    else:
        print("Warning, from argument type passed to expect_stanza: %s" % (type(xpaths)))

# list_of_xpaths: [(xpath, xpath), (xpath, xpath), (xpath)]
def expect_unordered(list_of_xpaths, xmpp, biboumi):
    formatted_list_of_xpaths = []
    for xpaths in list_of_xpaths:
        formatted_xpaths = []
        for xpath in xpaths:
            formatted_xpath = xpath.format_map(common_replacements)
            formatted_xpaths.append(formatted_xpath)
        formatted_list_of_xpaths.append(tuple(formatted_xpaths))
    # formatted_list_of_xpaths = [tuple(xpath.format_map(common_replacements) for xpath in xpaths) for xpaths in list_of_xpaths]

    expect_unordered_already_formatted(formatted_list_of_xpaths, xmpp, biboumi)

def expect_unordered_already_formatted(formatted_list_of_xpaths, xmpp, biboumi):
    xmpp.stanza_checker = partial(check_list_of_xpath, formatted_list_of_xpaths, xmpp)

def log_message(message, xmpp, biboumi):
    print("[33;1m%s[0m" % (message,))
    asyncio.get_event_loop().call_soon(xmpp.run_scenario)


class BiboumiTest:
    """
    Spawns a biboumi process and a fake XMPP Component that will run a
    Scenario.  It redirects the outputs of the subprocess into separated
    files, and detects any failure in the running of the scenario.
    """

    def __init__(self, scenario, expected_code=0):
        self.scenario = scenario
        self.expected_code = expected_code

    def run(self):
        with_valgrind = os.environ.get("E2E_WITH_VALGRIND") is not None
        print("Running scenario: [33;1m%s[0m%s" % (self.scenario.name, " (with valgrind)" if with_valgrind else ''))
        # Redirect the slixmpp logging into a specific file
        output_filename = "slixmpp_%s_output.txt" % (self.scenario.name,)
        with open(output_filename, "w"):
            pass
        logging.basicConfig(level=logging.DEBUG,
                            format='%(levelname)-8s %(message)s',
                            filename=output_filename)

        with open("test.conf", "w") as fd:
            fd.write(confs[self.scenario.conf])

        try:
            os.remove("e2e_test.sqlite")
        except FileNotFoundError:
            pass

        # Start the XMPP component and biboumi
        biboumi = BiboumiRunner(self.scenario.name)
        xmpp = XMPPComponent(self.scenario, biboumi)
        asyncio.get_event_loop().run_until_complete(biboumi.start())

        asyncio.get_event_loop().call_soon(xmpp.run_scenario)

        xmpp.process()
        code = asyncio.get_event_loop().run_until_complete(biboumi.wait())
        xmpp.biboumi = None
        self.scenario.steps.clear()
        failed = False
        if not xmpp.failed:
            if code != self.expected_code:
                xmpp.error("Wrong return code from biboumi's process: %d" % (code,))
                failed = True
            else:
                print("[32;1mSuccess![0m")
        else:
            failed = True

        xmpp.saved_values.clear()

        if xmpp.server:
            xmpp.accepting_server.close()

        return not failed


confs = {
'basic':
"""hostname=biboumi.localhost
password=coucou
db_name=e2e_test.sqlite
port=8811
admin=admin@example.com""",

'fixed_server':
"""hostname=biboumi.localhost
password=coucou
db_name=e2e_test.sqlite
port=8811
fixed_irc_server=irc.localhost
admin=admin@example.com
"""}

common_replacements = {
    'irc_server_one': 'irc.localhost@biboumi.localhost',
    'irc_host_one': 'irc.localhost',
    'biboumi_host': 'biboumi.localhost',
    'resource_one': 'resource1',
    'resource_two': 'resource2',
    'nick_one': 'Nick',
    'jid_one': 'first@example.com',
    'jid_two': 'second@example.com',
    'jid_admin': 'admin@example.com',
    'nick_two': 'Bobby',
    'lower_nick_one': 'nick',
    'lower_nick_two': 'bobby',
}


def handshake_sequence():
    return (partial(expect_stanza, "//handshake"),
            partial(send_stanza, "<handshake xmlns='jabber:component:accept'/>"))


def connection_begin_sequence(irc_host, jid):
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
    )


def connection_end_sequence(irc_host, jid):
    jid = jid.format_map(common_replacements)
    xpath    = "/message[@to='" + jid + "'][@from='irc.localhost@biboumi.localhost']/body[text()='%s']"
    xpath_re = "/message[@to='" + jid + "'][@from='irc.localhost@biboumi.localhost']/body[re:test(text(), '%s')]"
    return (
    partial(expect_stanza,
            xpath_re % (r'^%s: Your host is .*$' % irc_host)),
    partial(expect_stanza,
            xpath_re % (r'^%s: This server was created .*$' % irc_host)),
    partial(expect_stanza,
            xpath_re % (r'^%s: There are \d+ users and \d+ invisible on \d+ servers$' % irc_host)),
    partial(expect_stanza,
            xpath_re % (r'^%s: \d+ unknown connection\(s\)$' % irc_host), optional=True),
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


def connection_sequence(irc_host, jid):
    return connection_begin_sequence(irc_host, jid) + connection_end_sequence(irc_host, jid)


def extract_attribute(xpath, name, stanza):
    matched = match(stanza, xpath)
    return matched[0].get(name)


def save_value(name, func, stanza, xmpp):
    xmpp.saved_values[name] = func(stanza)


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
        Scenario("virtual_channel_join",
                 [
                     handshake_sequence(),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='%{irc_server_one}/{nick_one}' />"),
                     connection_begin_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='%{irc_server_one}'][@type='groupchat']/subject[re:test(text(), '^This is a virtual channel.*$')]"),
                     connection_end_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                 ]),
        Scenario("channel_join_with_two_users",
                 [
                     handshake_sequence(),
                     # First user joins
                     partial(log_message,
                             "First user joins"),
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
                     partial(log_message,
                             "Second user joins"),
                     partial(send_stanza,
                             "<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     connection_sequence("irc.localhost", '{jid_two}/{resource_one}'),
                     partial(expect_unordered, [
                         ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@role='participant'][@jid='~bobby@localhost']",),
                         ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",),
                         ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",
                         "/presence/muc_user:x/muc_user:status[@code='110']",),
                         ("/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]",),
                         ]),
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
                             "/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']"),
                     # Our own presence
                     partial(expect_stanza,
                             ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/subject[text()='TOPIC TEST']"),
                 ]),
        Scenario("channel_basic_join_on_fixed_irc_server",
                 [
                     handshake_sequence(),
                     partial(send_stanza,
                     "<presence from='{jid_one}/{resource_one}' to='#zgeg@{biboumi_host}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                     "/message/body[text()='Mode #zgeg [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                     ("/presence[@to='{jid_one}/{resource_one}'][@from='#zgeg@{biboumi_host}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                     "/presence/muc_user:x/muc_user:status[@code='110']")
                     ),
                     partial(expect_stanza, "/message[@from='#zgeg@{biboumi_host}'][@type='groupchat']/subject[not(text())]"),
                 ], conf='fixed_server'
                 ),
        Scenario("list_adhoc",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='get' id='idwhatever' from='{jid_one}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                                             "/iq/disco_items:query/disco_items:item[3]")),
                 ]),
        Scenario("list_admin_adhoc",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='get' id='idwhatever' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                                             "/iq/disco_items:query/disco_items:item[5]")),
                 ]),
        Scenario("list_adhoc_fixed_server",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='get' id='idwhatever' from='{jid_one}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                                             "/iq/disco_items:query/disco_items:item[3]")),
                 ], conf='fixed_server'),
        Scenario("list_admin_adhoc_fixed_server",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='get' id='idwhatever' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                                             "/iq/disco_items:query/disco_items:item[5]")),
                 ], conf='fixed_server'),


        Scenario("list_adhoc_irc",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='get' id='idwhatever' from='{jid_one}/{resource_one}' to='{irc_host_one}@{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                                             "/iq/disco_items:query/disco_items:item[1]")),
                 ]),
        Scenario("list_adhoc_irc_fixed_server",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='get' id='idwhatever' from='{jid_one}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                                             "/iq/disco_items:query/disco_items:item[4]")),
                 ], conf='fixed_server'),
        Scenario("list_admin_adhoc_irc_fixed_server",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='get' id='idwhatever' from='{jid_admin}/{resource_one}' to='{biboumi_host}'><query xmlns='http://jabber.org/protocol/disco#items' node='http://jabber.org/protocol/commands' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/disco_items:query[@node='http://jabber.org/protocol/commands']",
                                             "/iq/disco_items:query/disco_items:item[6]")),
                 ], conf='fixed_server'),

        Scenario("execute_hello_adhoc_command",
                 [
                     handshake_sequence(),
                     partial(send_stanza, "<iq type='set' id='hello-command1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='hello' action='execute' /></iq>"),
                     partial(expect_stanza, ("/iq[@type='result']/commands:command[@node='hello'][@sessionid][@status='executing']",
                                             "/iq/commands:command/dataform:x[@type='form']/dataform:title[text()='Configure your name.']",
                                             "/iq/commands:command/dataform:x[@type='form']/dataform:instructions[text()='Please provide your name.']",
                                             "/iq/commands:command/dataform:x[@type='form']/dataform:field[@type='text-single']/dataform:required",
                                             "/iq/commands:command/commands:actions/commands:next",
                                             ),
                                             after = partial(save_value, "sessionid", partial(extract_attribute, "/iq[@type='result']/commands:command[@node='hello']", "sessionid"))

                             ),
                     partial(send_stanza, "<iq type='set' id='hello-command2' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='hello' sessionid='{sessionid}' action='next'><x xmlns='jabber:x:data' type='submit'><field var='name'><value>COUCOU</value></field></x></command></iq>"),
                     partial(expect_stanza, "/iq[@type='result']/commands:command[@node='hello'][@status='completed']/commands:note[@type='info'][text()='Hello COUCOU!']")
                 ]),
        Scenario("multisessionnick",
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
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_one}']/subject[not(text())]"),

                     # The other resources joins the same room, with the same nick
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     # We receive our own join
                     partial(expect_unordered,
                             [("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                              "/presence/muc_user:x/muc_user:status[@code='110']"),
                              ("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]",)]
                             ),

                     # A different user joins the same room
                     partial(send_stanza,
                             "<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     connection_sequence("irc.localhost", '{jid_two}/{resource_one}'),

                     partial(expect_unordered, [
                                ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']",),
                                ("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_two}']",)
                                ]
                             ),

                     partial(expect_unordered, [
                         ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']",),
                         ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']",
                          "/presence/muc_user:x/muc_user:status[@code='110']",),
                          ]
                         ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

                     # That second user sends a private message to the first one
                     partial(send_stanza, "<message from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' type='chat'><body>RELLO</body></message>"),
                     # Message is received with a server-wide JID, by the two resources behind nick_one
                     partial(expect_stanza, ("/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='RELLO']",
                                             "/message/hints:no-copy",
                                             "/message/carbon:private")),
                     partial(expect_stanza, "/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_two}'][@type='chat']/body[text()='RELLO']"),

                     # One resource leaves the server entirely.
                     partial(send_stanza, "<presence type='unavailable' from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     # The leave is forwarded only to us
                     partial(expect_stanza,
                             ("/presence[@type='unavailable']/muc_user:x/muc_user:status[@code='110']",
                             "/presence/status[text()='Biboumi note: 1 resources are still in this channel.']")
                             ),
                     # The second user sends two new private messages to the first user
                     partial(send_stanza, "<message from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' type='chat'><body>first</body></message>"),
                     partial(send_stanza, "<message from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' type='chat'><body>second</body></message>"),
                     # The first user receives the two messages, on the connected resource, once each
                     partial(expect_stanza, "/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='first']"),
                     partial(expect_stanza, "/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='second']"),
                 ]),
        Scenario("channel_messages",
                 [
                     handshake_sequence(),
                     # First user joins
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

                     # Second user joins
                     partial(send_stanza,
                     "<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     connection_sequence("irc.localhost", '{jid_two}/{resource_one}'),
                     # Our presence, sent to the other user
                     partial(expect_unordered, [
                         ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",),
                         ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",),
                         ("/presence[@to='{jid_two}/{resource_one}'][@from='#foo%{irc_server_one}/{nick_two}']/muc_user:x/muc_user:item[@affiliation='none'][@jid='~bobby@localhost'][@role='participant']",
                          "/presence/muc_user:x/muc_user:status[@code='110']"),
                         ("/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]",)
                     ]),

                     # Send a channel message
                     partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
                     # Receive the message, forwarded to the two users
                     partial(expect_unordered, [
                         ("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']",),
                         ("/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_two}/{resource_one}'][@type='groupchat']/body[text()='coucou']",)
                         ]),

                     # Send a private message, to a in-room JID
                     partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' type='chat'><body>coucou in private</body></message>"),
                     # Message is received with a server-wide JID
                     partial(expect_stanza, "/message[@from='{lower_nick_one}%{irc_server_one}'][@to='{jid_two}/{resource_one}'][@type='chat']/body[text()='coucou in private']"),

                     # Respond to the message, to the server-wide JID
                     partial(send_stanza, "<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>yes</body></message>"),
                     # The response is received from the in-room JID
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}/{nick_two}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='yes']"),

                     ## Do the exact same thing, from a different chan,
                     # to check if the response comes from the right JID

                     # Join the virtual channel
                     partial(send_stanza,
                     "<presence from='{jid_one}/{resource_one}' to='%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza,
                     "/presence/muc_user:x/muc_user:status[@code='110']"),
                     partial(expect_stanza, "/message[@from='%{irc_server_one}'][@type='groupchat']/subject"),


                     # Send a private message, to a in-room JID
                     partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='%{irc_server_one}/{nick_two}' type='chat'><body>re in private</body></message>"),
                     # Message is received with a server-wide JID
                     partial(expect_stanza, "/message[@from='{lower_nick_one}%{irc_server_one}'][@to='{jid_two}/{resource_one}'][@type='chat']/body[text()='re in private']"),

                     # Respond to the message, to the server-wide JID
                     partial(send_stanza, "<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>re</body></message>"),
                     # The response is received from the in-room JID
                     partial(expect_stanza, "/message[@from='%{irc_server_one}/{nick_two}'][@to='{jid_one}/{resource_one}'][@type='chat']/body[text()='re']"),

                     # Now we leave the room, to check if the subsequent private messages are still received properly
                     partial(send_stanza,
                     "<presence from='{jid_one}/{resource_one}' to='%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(expect_stanza,
                     "/presence[@type='unavailable']/muc_user:x/muc_user:status[@code='110']"),

                     # The private messages from this nick should now come (again) from the server-wide JID
                     partial(send_stanza, "<message from='{jid_two}/{resource_one}' to='{lower_nick_one}%{irc_server_one}' type='chat'><body>hihihoho</body></message>"),
                     partial(expect_stanza,
                     "/message[@from='{lower_nick_two}%{irc_server_one}'][@to='{jid_one}/{resource_one}']"),
                 ]
                 ),
                Scenario("encoded_channel_join",
                 [
                     handshake_sequence(),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#biboumi\\40louiz.org\\3a80%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #biboumi@louiz.org:80 [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#biboumi\\40louiz.org\\3a80%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                             "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#biboumi\\40louiz.org\\3a80%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
                 ]),
                Scenario("self_ping_on_real_channel",
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

                     # Send a ping to ourself
                     partial(send_stanza,
                             "<iq type='get' from='{jid_one}/{resource_one}' id='first_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
                     # We receive our own ping request,
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to='{jid_one}/{resource_one}'][@id='gnip_tsrif']"),
                     # Respond to the request
                     partial(send_stanza,
                             "<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='gnip_tsrif' from='{jid_one}/{resource_one}'/>"),
                     partial(expect_stanza,
                             "/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_ping']"),

                     # Now join the same room, from the same bare JID, behind the same nick
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

                     # And re-send a self ping
                     partial(send_stanza,
                             "<iq type='get' from='{jid_one}/{resource_one}' id='second_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
                     # We receive our own ping request. Note that we don't know the to value, it could be one of our two resources.
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to][@id='gnip_dnoces']",
                             after = partial(save_value, "to", partial(extract_attribute, "/iq", "to"))),
                     # Respond to the request, using the extracted 'to' value as our 'from'
                     partial(send_stanza,
                             "<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='gnip_dnoces' from='{to}'/>"),
                     partial(expect_stanza,
                             "/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='second_ping']"),
                     ## And re-do exactly the same thing, just change the resource initiating the self ping
                     partial(send_stanza,
                             "<iq type='get' from='{jid_one}/{resource_two}' id='third_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to][@id='gnip_driht']",
                             after = partial(save_value, "to", partial(extract_attribute, "/iq", "to"))),
                     partial(send_stanza,
                             "<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='gnip_driht' from='{to}'/>"),
                     partial(expect_stanza,
                             "/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_two}'][@id='third_ping']"),

                 ]),
                Scenario("simple_kick",
                [
                     handshake_sequence(),
                     # First user joins
                     partial(send_stanza,
                     "<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence/muc_user:x/muc_user:status[@code='110']"),
                     partial(expect_stanza, "/message[@type='groupchat']/subject"),

                     # Second user joins
                     partial(send_stanza,
                     "<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     connection_sequence("irc.localhost", '{jid_two}/{resource_one}'),
                     partial(expect_unordered, [
                         ("/presence/muc_user:x/muc_user:item[@affiliation='none'][@role='participant']",),
                         ("/presence/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",),
                         ("/presence/muc_user:x/muc_user:status[@code='110']",),
                         ("/message/subject",),
                         ]),

                     # Moderator kicks participant
                     partial(log_message, "Moderator kicks participant"),
                     partial(send_stanza,
                     "<iq id='kick1' to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item nick='{nick_two}' role='none'><reason>reported</reason></item></query></iq>"),
                     partial(log_message, "Presence is sent to everyone"),
                     partial(expect_unordered, [
                             ("/presence[@type='unavailable'][@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
                              "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
                              "/presence/muc_user:x/muc_user:status[@code='307']",
                              "/presence/muc_user:x/muc_user:status[@code='110']"
                             ),
                             ("/presence[@type='unavailable'][@to='{jid_one}/{resource_one}']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
                              "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
                              "/presence/muc_user:x/muc_user:status[@code='307']",
                              ),
                             ("/iq[@id='kick1'][@type='result']",),
                     ]),
                ]),
                Scenario("multisession_kick",
                 [
                     handshake_sequence(),
                     # First user joins
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence/muc_user:x/muc_user:status[@code='110']"),
                     partial(expect_stanza, "/message[@type='groupchat']/subject"),

                     # Second user joins, fprom two resources
                     partial(send_stanza,
                             "<presence from='{jid_two}/{resource_one}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     connection_sequence("irc.localhost", '{jid_two}/{resource_one}'),
                     partial(expect_unordered, [
                         ("/presence/muc_user:x/muc_user:item[@affiliation='none'][@role='participant']",),
                         ("/presence/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",),
                         ("/presence/muc_user:x/muc_user:status[@code='110']",),
                         ("/message/subject",),
                     ]),

                     partial(send_stanza,
                             "<presence from='{jid_two}/{resource_two}' to='#foo%{irc_server_one}/{nick_two}' />"),
                     partial(expect_stanza,
                             "/presence[@to='{jid_two}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_two}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_two}']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_two}/{resource_two}']/subject[not(text())]"),

                     # Moderator kicks participant
                     partial(log_message, "Moderator kicks participant"),
                     partial(send_stanza,
                             "<iq id='kick1' to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set'><query xmlns='http://jabber.org/protocol/muc#admin'><item nick='{nick_two}' role='none'><reason>reported</reason></item></query></iq>"),
                     partial(log_message, "Unavailable presence is sent to the two resources"),
                     partial(expect_unordered, [
                             ("/presence[@type='unavailable'][@to='{jid_two}/{resource_one}']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
                              "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
                              "/presence/muc_user:x/muc_user:status[@code='307']",
                              "/presence/muc_user:x/muc_user:status[@code='110']"
                              ),
                             ("/presence[@type='unavailable'][@to='{jid_two}/{resource_two}']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
                              "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
                              "/presence/muc_user:x/muc_user:status[@code='307']",
                              "/presence/muc_user:x/muc_user:status[@code='110']"
                              ),
                             ("/presence[@type='unavailable']/muc_user:x/muc_user:item[@role='none']/muc_user:actor[@nick='{nick_one}']",
                              "/presence/muc_user:x/muc_user:item/muc_user:reason[text()='reported']",
                              "/presence/muc_user:x/muc_user:status[@code='307']",
                              ),
                             ("/iq[@id='kick1'][@type='result']",),
                             ]),
                 ]),
                Scenario("self_version",
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

                     # Send a version request to ourself
                     partial(send_stanza,
                             "<iq type='get' from='{jid_one}/{resource_one}' id='first_version' to='#foo%{irc_server_one}/{nick_one}'><query xmlns='jabber:iq:version' /></iq>"),
                     # We receive our own request,
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to='{jid_one}/{resource_one}']",
                             after = partial(save_value, "id", partial(extract_attribute, "/iq", 'id'))),
                     # Respond to the request
                     partial(send_stanza,
                             "<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='{id}' from='{jid_one}/{resource_one}'><query xmlns='jabber:iq:version'><name>e2e test</name><version>1.0</version><os>Fedora</os></query></iq>"),
                     partial(expect_stanza,
                             "/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_version']/version:query/version:name[text()='e2e test (through the biboumi gateway) 1.0 Fedora']"),

                     # Now join the same room, from the same bare JID, behind the same nick
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

                     # And re-send a self ping
                     partial(send_stanza,
                             "<iq type='get' from='{jid_one}/{resource_two}' id='second_version' to='#foo%{irc_server_one}/{nick_one}'><query xmlns='jabber:iq:version' /></iq>"),
                     # We receive our own request. Note that we don't know the to value, it could be one of our two resources.
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to]",
                             after = (partial(save_value, "to", partial(extract_attribute, "/iq", "to")),
                                      partial(save_value, "id", partial(extract_attribute, "/iq", "id")))),
                     # Respond to the request, using the extracted 'to' value as our 'from'
                     partial(send_stanza,
                             "<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='{id}' from='{to}'><query xmlns='jabber:iq:version'><name>e2e test</name><version>1.0</version><os>Fedora</os></query></iq>"),
                     partial(expect_stanza,
                             "/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_two}'][@id='second_version']"),

                     # And do exactly the same thing, but initiated by the other resource
                     partial(send_stanza,
                             "<iq type='get' from='{jid_one}/{resource_one}' id='second_version' to='#foo%{irc_server_one}/{nick_one}'><query xmlns='jabber:iq:version' /></iq>"),
                     # We receive our own request. Note that we don't know the to value, it could be one of our two resources.
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to]",
                             after = (partial(save_value, "to", partial(extract_attribute, "/iq", "to")),
                                      partial(save_value, "id", partial(extract_attribute, "/iq", "id")))),
                     # Respond to the request, using the extracted 'to' value as our 'from'
                     partial(send_stanza,
                             "<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='{id}' from='{to}'><query xmlns='jabber:iq:version'><name>e2e test</name><version>1.0</version><os>Fedora</os></query></iq>"),
                     partial(expect_stanza,
                             "/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='second_version']"),
                 ]),
                Scenario("version_on_global_nick",
                [
                    partial(log_message, "Joining the channel"),
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

                    partial(log_message, "Send a version request to ourself"),
                     partial(send_stanza,
                             "<iq type='get' from='{jid_one}/{resource_one}' id='first_version' to='{lower_nick_one}%{irc_server_one}'><query xmlns='jabber:iq:version' /></iq>"),

                     partial(log_message, "Receive our own request"),
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='get'][@to='{jid_one}/{resource_one}']",
                             after = partial(save_value, "id", partial(extract_attribute, "/iq", 'id'))),
                     partial(log_message, "Respond to the request"),
                     partial(send_stanza,
                             "<iq type='result' to='{lower_nick_one}%{irc_server_one}' id='{id}' from='{jid_one}/{resource_one}'><query xmlns='jabber:iq:version'><name>e2e test</name><version>1.0</version><os>Fedora</os></query></iq>"),
                     partial(expect_stanza,
                             "/iq[@from='{lower_nick_one}%{irc_server_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_version']/version:query/version:name[text()='e2e test (through the biboumi gateway) 1.0 Fedora']"),

                ]),
                Scenario("self_invite",
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
                    partial(send_stanza,
                            "<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}'><x xmlns='http://jabber.org/protocol/muc#user'><invite to='{nick_one}'/></x></message>"),
                    partial(expect_stanza,
                            "/message/body[text()='{nick_one} is already on channel #foo']")
                ]),
                Scenario("client_error",
                [
                    handshake_sequence(),
                    # First resource
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

                    # Second resource, same channel
                    partial(send_stanza,
                            "<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
                    partial(expect_stanza,
                            ("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                             "/presence/muc_user:x/muc_user:status[@code='110']")
                            ),
                    partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

                    # Now the first resource has an error
                    partial(send_stanza,
                            "<message from='{jid_one}/{resource_one}' to='#foo%%{irc_server_one}/{nick_one}' type='error'><error type='cancel'><recipient-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></message>"),
                    # Receive a leave only to the leaving resource
                    partial(expect_stanza,
                            ("/presence[@type='unavailable'][@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}']/muc_user:x/muc_user:status[@code='110']",
                             "/presence/status[text()='Biboumi note: 1 resources are still in this channel.']")
                            ),
                ]),
                Scenario("simple_mam",
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

                    # Send two channel messages
                    partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
                    partial(expect_stanza, "/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']"),

                    partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou 2</body></message>"),
                    partial(expect_stanza, "/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou 2']"),

                    # Retrieve the complete archive
                    partial(send_stanza, "<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id1'><query xmlns='urn:xmpp:mam:1' queryid='qid1' /></iq>"),

                    partial(expect_stanza,
                            ("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                            "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou']")
                            ),
                    partial(expect_stanza,
                            ("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                            "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo%{irc_server_one}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 2']")
                            ),

                    partial(expect_stanza,
                            "/iq[@type='result'][@id='id1'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']"),

                    # Retrieve an empty archive by specifying an early “end” date
                    partial(send_stanza, """<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id2'>
                    <query xmlns='urn:xmpp:mam:1' queryid='qid2'>
                    <x xmlns='jabber:x:data' type='submit'>
                    <field var='FORM_TYPE' type='hidden'> <value>urn:xmpp:mam:1</value></field>
                    <field var='end'><value>2000-06-07T00:00:00Z</value></field>
                    </x>
                    </query></iq>"""),

                    partial(expect_stanza,
                            "/iq[@type='result'][@id='id2'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']"),

                    # Retrieve an empty archive by specifying a late “start” date
                    # (note that this test will break in ~1000 years)
                    partial(send_stanza, """<iq to='#foo%{irc_server_one}' from='{jid_one}/{resource_one}' type='set' id='id3'>
                    <query xmlns='urn:xmpp:mam:1' queryid='qid3'>
                    <x xmlns='jabber:x:data' type='submit'>
                    <field var='FORM_TYPE' type='hidden'> <value>urn:xmpp:mam:1</value></field>
                    <field var='start'><value>3016-06-07T00:00:00Z</value></field>
                    </x>
                    </query></iq>"""),

                    partial(expect_stanza,
                            "/iq[@type='result'][@id='id3'][@from='#foo%{irc_server_one}'][@to='{jid_one}/{resource_one}']"),
                ]),
        Scenario("mam_on_fixed_server",
                 [
                     handshake_sequence(),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo@{biboumi_host}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo@{biboumi_host}'][@type='groupchat']/subject[not(text())]"),

                     partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' type='groupchat'><body>coucou</body></message>"),
                     partial(expect_stanza, "/message[@from='#foo@{biboumi_host}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']"),

                     partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' type='groupchat'><body>coucou 2</body></message>"),
                     partial(expect_stanza, "/message[@from='#foo@{biboumi_host}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou 2']"),

                     # Retrieve the complete archive
                     partial(send_stanza, "<iq to='#foo@{biboumi_host}' from='{jid_one}/{resource_one}' type='set' id='id1'><query xmlns='urn:xmpp:mam:1' queryid='qid1' /></iq>"),

                     partial(expect_stanza,
                             ("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                              "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo@{biboumi_host}/{nick_one}'][@type='groupchat']/client:body[text()='coucou']")
                             ),
                     partial(expect_stanza,
                             ("/message/mam:result[@queryid='qid1']/forward:forwarded/delay:delay",
                              "/message/mam:result[@queryid='qid1']/forward:forwarded/client:message[@from='#foo@{biboumi_host}/{nick_one}'][@type='groupchat']/client:body[text()='coucou 2']")
                             ),
                 ], conf="fixed_server"),
        Scenario("channel_history_on_fixed_server",
                 [
                     handshake_sequence(),
                     # First user join
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_one}'][@from='#foo@{biboumi_host}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='~nick@localhost'][@role='moderator']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     partial(expect_stanza, "/message[@from='#foo@{biboumi_host}'][@type='groupchat']/subject[not(text())]"),

                     # Send one channel message
                     partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' type='groupchat'><body>coucou</body></message>"),
                     partial(expect_stanza, "/message[@from='#foo@{biboumi_host}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']"),

                     # Second user joins
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_two}' to='#foo@{biboumi_host}/{nick_one}' />"),
                     # connection_sequence("irc.localhost", '{jid_one}/{resource_two}'),
                     # partial(expect_stanza,
                     #         "/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
                     partial(expect_stanza,
                             ("/presence[@to='{jid_one}/{resource_two}'][@from='#foo@{biboumi_host}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='~nick@localhost'][@role='moderator']",
                              "/presence/muc_user:x/muc_user:status[@code='110']")
                             ),
                     # Receive the history message
                     partial(expect_stanza, ("/message[@from='#foo@{biboumi_host}/{nick_one}']/body[text()='coucou']",
                                             "/message/delay:delay[@from='#foo@{biboumi_host}']")),

                     partial(expect_stanza, "/message[@from='#foo@{biboumi_host}'][@type='groupchat']/subject[not(text())]"),
                 ], conf="fixed_server"),
    Scenario("channel_history",
             [
                 handshake_sequence(),
                 # First user join
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

                 # Send one channel message
                 partial(send_stanza, "<message from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' type='groupchat'><body>coucou</body></message>"),
                 partial(expect_stanza, "/message[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='groupchat']/body[text()='coucou']"),

                 # Second user joins
                 partial(send_stanza,
                         "<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
                 # connection_sequence("irc.localhost", '{jid_one}/{resource_two}'),
                 # partial(expect_stanza,
                 #         "/message/body[text()='Mode #foo [+nt] by {irc_host_one}']"),
                 partial(expect_stanza,
                         ("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@jid='~nick@localhost'][@role='moderator']",
                          "/presence/muc_user:x/muc_user:status[@code='110']")
                         ),
                 # Receive the history message
                 partial(expect_stanza, ("/message[@from='#foo%{irc_server_one}/{nick_one}']/body[text()='coucou']",
                                         "/message/delay:delay[@from='#foo%{irc_server_one}']")),

                 partial(expect_stanza, "/message[@from='#foo%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),
             ]),
        Scenario("simple_channel_list",
                 [
                     handshake_sequence(),

                     partial(log_message, "Join first channel #foo"),
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

                     partial(log_message, "Join second channel #bar"),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #bar [+nt] by {irc_host_one}']"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message[@from='#bar%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

                     partial(log_message, "Request the whole channel list"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'/></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#foo%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#bar%{irc_server_one}']"
                     ))
                 ]),
        Scenario("channel_list_with_rsm",
                 [
                     handshake_sequence(),

                     partial(log_message, "Join first channel #foo"),
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

                     partial(log_message, "Join second channel #bar"),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#bar%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #bar [+nt] by {irc_host_one}']"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message[@from='#bar%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

                     partial(log_message, "Join third channel #coucou"),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#coucou%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza,
                             "/message/body[text()='Mode #coucou [+nt] by {irc_host_one}']"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message[@from='#coucou%{irc_server_one}'][@type='groupchat']/subject[not(text())]"),

                     partial(log_message, "Request with max=0"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>0</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         )),

                     partial(log_message, "Request with max=2"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>2</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#bar%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#coucou%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#bar%{irc_server_one}'][@index='0']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#coucou%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"
                     )),

                     partial(log_message, "Request with max=12"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>12</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#bar%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#coucou%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#foo%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#bar%{irc_server_one}'][@index='0']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#foo%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"
                     )),

                     partial(log_message, "Request with max=1 after=#bar"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#bar%{irc_server_one}</after><max>1</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#coucou%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#coucou%{irc_server_one}'][@index='1']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#coucou%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"
                     )),

                     partial(log_message, "Request with max=1 after=#bar"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id1' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#bar%{irc_server_one}</after><max>1</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#coucou%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#coucou%{irc_server_one}'][@index='1']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#coucou%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:count[text()='3']"
                     ))
                 ]),
                Scenario("complete_channel_list_with_pages_of_3",
                 [
                     handshake_sequence(),

                     partial(log_message, "Join 10 channels"),
                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#aaa%{irc_server_one}/{nick_one}' />"),
                     connection_sequence("irc.localhost", '{jid_one}/{resource_one}'),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#bbb%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#ccc%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#ddd%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#eee%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#fff%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#ggg%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#hhh%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#iii%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(send_stanza,
                             "<presence from='{jid_one}/{resource_one}' to='#jjj%{irc_server_one}/{nick_one}' />"),
                     partial(expect_stanza, "/message"),
                     partial(expect_stanza, "/presence"),
                     partial(expect_stanza, "/message"),

                     partial(log_message, "Request the first page, with a limit of 3"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><max>3</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#aaa%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#bbb%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#ccc%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#aaa%{irc_server_one}'][@index='0']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#ccc%{irc_server_one}']"
                     )),

                     partial(log_message, "Request subsequent pages"),
                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#ccc%{irc_server_one}</after><max>3</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#ddd%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#eee%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#fff%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#ddd%{irc_server_one}'][@index='3']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#fff%{irc_server_one}']"
                     )),

                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#fff%{irc_server_one}</after><max>3</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#ggg%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#hhh%{irc_server_one}']",
                         "/iq/disco_items:query/disco_items:item[@jid='#iii%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#ggg%{irc_server_one}'][@index='6']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#iii%{irc_server_one}']"
                     )),

                     partial(send_stanza, "<iq from='{jid_one}/{resource_one}' id='id' to='{irc_server_one}' type='get'><query xmlns='http://jabber.org/protocol/disco#items'><set xmlns='http://jabber.org/protocol/rsm'><after>#iii%{irc_server_one}</after><max>3</max></set></query></iq>"),
                     partial(expect_stanza, (
                         "/iq[@type='result']/disco_items:query",
                         "/iq/disco_items:query/disco_items:item[@jid='#jjj%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:first[text()='#jjj%{irc_server_one}'][@index='9']",
                         "/iq/disco_items:query/rsm:set/rsm:last[text()='#jjj%{irc_server_one}']",
                         "/iq/disco_items:query/rsm:set/rsm:count[text()='10']"
                     )),

                     partial(log_message, "Leaving the 10 channels"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#aaa%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#bbb%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#ccc%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#ddd%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#eee%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#fff%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#ggg%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#hhh%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#iii%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(send_stanza, "<presence from='{jid_one}/{resource_one}' to='#jjj%{irc_server_one}/{nick_one}' type='unavailable' />"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']"),
                     partial(expect_stanza, "/presence[@type='unavailable']")
                ]),
                Scenario("muc_traffic_info",
                [
                     handshake_sequence(),

                     partial(send_stanza,
                             "<iq from='{jid_one}/{resource_one}' to='#foo%{irc_server_one}' id='1' type='get'><query xmlns='http://jabber.org/protocol/disco#info' node='http://jabber.org/protocol/muc#traffic'/></iq>"),
                     partial(expect_stanza, "/iq[@type='result']/disco_info:query[@node='http://jabber.org/protocol/muc#traffic']"),
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

    for s in scenarios:
        test = BiboumiTest(s)
        if not test.run():
            print("You can check the files slixmpp_%s_output.txt and biboumi_%s_output.txt to help you debug." %
                  (s.name, s.name))
            failures += 1

    print("Waiting for irc server to exit…")
    irc.stop()
    asyncio.get_event_loop().run_until_complete(irc.wait())

    if failures:
        print("%d test%s failed, please fix %s." % (failures, 's' if failures > 1 else '',
                                                    'them' if failures > 1 else 'it'))
        sys.exit(1)
    else:
        print("All tests passed successfully")
