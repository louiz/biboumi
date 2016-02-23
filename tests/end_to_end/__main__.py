#!/usr/bin/env python3

import slixmpp
import asyncio
import logging
import signal
import atexit
import sys
from functools import partial


class MatchAll(slixmpp.xmlstream.matcher.base.MatcherBase):
    """match everything"""

    def match(self, xml):
        return True


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
        self.expected_xpath = None
        self.failed = False
        self.accepting_server = None

    def error(self, message):
        print("[31;1mFailure[0m: %s" % (message,))
        self.scenario.steps = []
        self.failed = True

    def on_end_session(self, event):
        self.loop.stop()

    def handle_incoming_stanza(self, stanza):
        if self.expected_xpath:
            matched = slixmpp.xmlstream.matcher.xpath.MatchXPath(self.expected_xpath).match(stanza)
            if not matched:
                self.error("Received stanza “%s” did not match expected xpath “%s”" % (stanza, self.expected_xpath))
            self.expected_xpath = None
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


class Scenario:
    """Defines a list of actions that are executed in sequence, until one of
    them throws an exception, or until the end.  An action can be something
    like “send a stanza”, “receive the next stanza and check that it matches
    the given XPath”, “send a signal”, “wait for the end of the process”,
    etc
    """

    def __init__(self, name, steps):
        """
        Steps is a list of 2-tuple:
        [(action, answer), (action, answer)]
        """
        self.name = name
        self.steps = steps


class BiboumiRunner:
    def __init__(self, name, with_valgrind):
        self.name = name
        self.fd = open("biboumi_%s_output.txt" % (name,), "w")
        if with_valgrind:
            self.create = asyncio.create_subprocess_exec("valgrind", "--leak-check=full", "--show-leak-kinds=all", "--errors-for-leak-kinds=all", "--error-exitcode=16", "./biboumi", "test.conf", stdin=None, stdout=self.fd,
                                                         stderr=self.fd, loop=None, limit=None)
        else:
            self.create = asyncio.create_subprocess_exec("./biboumi", "test.conf", stdin=None, stdout=self.fd,
                                                         stderr=self.fd, loop=None, limit=None)
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


def send_stanza(stanza, xmpp, biboumi):
    xmpp.send_raw(stanza)
    asyncio.get_event_loop().call_soon(xmpp.run_scenario)


def expect_stanza(xpath, xmpp, biboumi):
    xmpp.expected_xpath = xpath


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


if __name__ == '__main__':

    atexit.register(asyncio.get_event_loop().close)

    # Start the test component, accepting connections on the configured
    # port.
    scenarios = (
        Scenario("basic_handshake_success",
                 [
                     partial(expect_stanza, "{jabber:component:accept}handshake"),
                     partial(send_stanza, "<handshake xmlns='jabber:component:accept'/>"),
                 ]),
        Scenario("channel_join",
                 [
                     partial(expect_stanza, "{jabber:component:accept}handshake"),
                     partial(send_stanza, "<handshake xmlns='jabber:component:accept'/>"),
                     partial(send_stanza, "<presence from='me@example.com/Nick' to='#foo%irc.localhost@biboumi.localhost' />"),
                     partial(expect_stanza, "{jabber:component:accept}message/body"),
                 ]),
    )

    failures = 0

    print("Running %s checks for biboumi." % (len(scenarios)))

    for scenario in scenarios:
        test = BiboumiTest(scenario)
        if not test.run(False):
            print("You can check the files slixmpp_%s_output.txt and biboumi_%s_output.txt to help you debug." %
                  (scenario.name, scenario.name))
            failures += 1

    if failures:
        print("%d test%s failed, please fix %s." % (failures, 's' if failures > 1 else '',
                                                    'them' if failures > 1 else 'it'))
        sys.exit(1)
    else:
        print("All tests passed successfully")
