#!/usr/bin/env python3

from functions import StanzaError, SkipStepError

import collections
import lxml.etree
import importlib
import datetime
import slixmpp
import asyncio
import logging
import signal
import atexit
import time
import sys
import io
import os
from functools import partial
from slixmpp.xmlstream.matcher.base import MatcherBase

if not hasattr(asyncio, "ensure_future"):
    asyncio.ensure_future = getattr(asyncio, "async")

class MatchAll(MatcherBase):
    """match everything"""

    def match(self, xml):
        return True


class Scenario:
    """Defines a list of actions that are executed in sequence, until one of
    them throws an exception, or until the end.  An action can be something
    like “send a stanza”, “receive the next stanza and check that it matches
    the given XPath”, “send a signal”, “wait for the end of the process”,
    etc
    """

    def __init__(self, name, steps, conf):
        """
        Steps is a list of 2-tuple:
        [(action, answer), (action, answer)]
        """
        self.name = name
        self.steps = []
        self.conf = conf

        def unwrap_tuples(elements):
            """Yields all the value contained in the tuples, of tuples, of tuples…
            For example unwrap_tuples((1, 2, 3, (4, 5, (6,)))) will yield 1, 2, 3, 4, 5, 6
            This works with any depth"""
            if isinstance(elements, collections.abc.Iterable):
                for elem in elements:
                    yield from unwrap_tuples(elem)
            else:
                yield elements

        for step in unwrap_tuples(steps):
            self.steps.append(step)

class XMPPComponent(slixmpp.BaseXMPP):
    """
    XMPPComponent sending a “scenario” of stanzas, checking that the responses
    match the expected results.
    """

    def __init__(self, scenario, biboumi):
        super().__init__(jid="biboumi.localhost", default_ns="jabber:component:accept")
        self.is_component = True
        self.auto_authorize = None # Do not accept or reject subscribe requests automatically
        self.auto_subscribe = False
        self.stream_header = '<stream:stream %s %s from="%s" id="%s">' % (
            'xmlns="jabber:component:accept"',
            'xmlns:stream="%s"' % self.stream_ns,
            self.boundjid, self.new_id())
        self.stream_footer = "</stream:stream>"

        self.register_handler(slixmpp.Callback('Match All',
                                               MatchAll(None),
                                               self.handle_incoming_stanza))

        self.add_event_handler("session_end", self.on_end_session)

        asyncio.ensure_future(self.accept_routine())

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
            try:
                step(xmpp=self, biboumi=self.biboumi)
            except Exception as e:
                self.error(e)
                self.run_scenario()
        else:
            if self.biboumi:
                self.biboumi.stop()

    @asyncio.coroutine
    def accept_routine(self):
        self.accepting_server = yield from self.loop.create_server(lambda: self,
                                                                   "127.0.0.1", 8811, reuse_address=True)

    def check_stanza_against_all_expected_xpaths(self):
        pass


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
            found = True
            list_of_xpaths.pop(i)
            break

    if not found:
        raise StanzaError("Received stanza “%s” did not match any of the expected xpaths:\n%s" % (stanza, list_of_xpaths))

    if list_of_xpaths:
        step = partial(expect_unordered_already_formatted, list_of_xpaths)
        xmpp.scenario.steps.insert(0, step)


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


def save_current_timestamp_plus_delta(key, delta, message, xmpp):
    now_plus_delta = datetime.datetime.utcnow() + delta
    xmpp.saved_values[key] = now_plus_delta.strftime("%FT%T.967Z")

def sleep_for(duration, xmpp, biboumi):
    time.sleep(duration)
    asyncio.get_event_loop().call_soon(xmpp.run_scenario)

# list_of_xpaths: [(xpath, xpath), (xpath, xpath), (xpath)]
def expect_unordered(list_of_xpaths, xmpp, biboumi):
    formatted_list_of_xpaths = []
    for xpaths in list_of_xpaths:
        formatted_xpaths = []
        for xpath in xpaths:
            formatted_xpath = xpath.format_map(common_replacements)
            formatted_xpaths.append(formatted_xpath)
        formatted_list_of_xpaths.append(tuple(formatted_xpaths))
    expect_unordered_already_formatted(formatted_list_of_xpaths, xmpp, biboumi)

def expect_unordered_already_formatted(formatted_list_of_xpaths, xmpp, biboumi):
    xmpp.stanza_checker = partial(check_list_of_xpath, formatted_list_of_xpaths, xmpp)


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

        start_datetime = datetime.datetime.now()

        # Start the XMPP component and biboumi
        biboumi = BiboumiRunner(self.scenario.name)
        xmpp = XMPPComponent(self.scenario, biboumi)
        asyncio.get_event_loop().run_until_complete(biboumi.start())

        asyncio.get_event_loop().call_soon(xmpp.run_scenario)

        xmpp.process()
        code = asyncio.get_event_loop().run_until_complete(biboumi.wait())
        xmpp.biboumi = None
        self.scenario.steps.clear()

        delta = datetime.datetime.now() - start_datetime

        failed = False
        if not xmpp.failed:
            if code != self.expected_code:
                xmpp.error("Wrong return code from biboumi's process: %d" % (code,))
                failed = True
            else:
                print("[32;1mSuccess![0m ({}s)".format(round(delta.total_seconds(), 2)))
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
admin=admin@example.com
identd_port=1113
outgoing_bind=127.0.0.1""",

'fixed_server':
"""hostname=biboumi.localhost
password=coucou
db_name=e2e_test.sqlite
port=8811
fixed_irc_server=irc.localhost
admin=admin@example.com
identd_port=1113
""",

'persistent_by_default':
"""hostname=biboumi.localhost
password=coucou
db_name=e2e_test.sqlite
port=8811
persistent_by_default=true
""",}


def chan_name_from_jid(jid):
    return jid[1:jid.find('%')]

def get_scenarios(test_path, provided_scenar_names):
    scenarios = []
    for entry in os.scandir(os.path.join(test_path, "scenarios")):
        if entry.is_file() and not entry.name.startswith('.') and entry.name.endswith('.py'):
            module_name = entry.name[:-3]
            if provided_scenar_names and module_name not in provided_scenar_names:
                continue
            if module_name == "__init__" or (provided_scenar_names and module_name not in provided_scenar_names):
                continue
            module_full_path = "scenarios.{}".format(module_name)
            mod = importlib.import_module(module_full_path)
            conf = "basic"
            if hasattr(mod, "conf"):
                conf = mod.conf
            scenarios.append(Scenario(module_name, mod.scenario, conf))
    return scenarios


if __name__ == '__main__':
    atexit.register(asyncio.get_event_loop().close)

    provided_scenar_names = sys.argv[1:]
    scenarios = get_scenarios(os.path.abspath(os.path.dirname(__file__)), provided_scenar_names)

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

    number_of_checks = len([s for s in scenarios if s.name in provided_scenar_names]) if provided_scenar_names else len(scenarios)
    print("Running %s checks for biboumi." % number_of_checks)

    failures = 0
    for s in scenarios:
        test = BiboumiTest(s)
        if not test.run():
            print("You can check the files slixmpp_%s_output.txt and biboumi_%s_output.txt to help you debug." %
                  (s.name, s.name))
            failures += 1
        sys.stdout.flush()

    print("Waiting for irc server to exit…")
    irc.stop()
    asyncio.get_event_loop().run_until_complete(irc.wait())

    if failures:
        print("%d test%s failed, please fix %s." % (failures, 's' if failures > 1 else '',
                                                    'them' if failures > 1 else 'it'))
        sys.exit(1)
    else:
        print("All tests passed successfully")

