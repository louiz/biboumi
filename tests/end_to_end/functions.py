from functools import partial
import collections
import datetime
import asyncio
import time
import lxml
import io

common_replacements = {
    'irc_server_one': 'irc.localhost@biboumi.localhost',
    'irc_server_two': 'localhost@biboumi.localhost',
    'irc_host_one': 'irc.localhost',
    'irc_host_two': 'localhost',
    'biboumi_host': 'biboumi.localhost',
    'resource_one': 'resource1',
    'resource_two': 'resource2',
    'nick_one': 'Nick',
    'jid_one': 'first@example.com',
    'jid_two': 'second@example.com',
    'jid_admin': 'admin@example.com',
    'nick_two': 'Bobby',
    'nick_three': 'Bernard',
    'lower_nick_one': 'nick',
    'lower_nick_two': 'bobby',
}

class SkipStepError(Exception):
    """
    Raised by a step when it needs to be skiped, by running
    the next available step immediately.
    """
    pass

class StanzaError(Exception):
    """
    Raised when a step fails.
    """
    pass

def match(stanza, xpath):
    tree = lxml.etree.parse(io.StringIO(str(stanza)))
    matched = tree.xpath(xpath, namespaces={'re': 'http://exslt.org/regular-expressions',
                                            'muc_user': 'http://jabber.org/protocol/muc#user',
                                            'muc_owner': 'http://jabber.org/protocol/muc#owner',
                                            'muc': 'http://jabber.org/protocol/muc',
                                            'disco_info': 'http://jabber.org/protocol/disco#info',
                                            'muc_traffic': 'http://jabber.org/protocol/muc#traffic',
                                            'disco_items': 'http://jabber.org/protocol/disco#items',
                                            'commands': 'http://jabber.org/protocol/commands',
                                            'dataform': 'jabber:x:data',
                                            'version': 'jabber:iq:version',
                                            'mam': 'urn:xmpp:mam:2',
                                            'rms': 'http://jabber.org/protocol/rsm',
                                            'delay': 'urn:xmpp:delay',
                                            'forward': 'urn:xmpp:forward:0',
                                            'client': 'jabber:client',
                                            'rsm': 'http://jabber.org/protocol/rsm',
                                            'carbon': 'urn:xmpp:carbons:2',
                                            'hints': 'urn:xmpp:hints',
                                            'stanza': 'urn:ietf:params:xml:ns:xmpp-stanzas',
                                            'stable_id': 'urn:xmpp:sid:0'})
    return matched

def check_xpath(xpaths, xmpp, after, stanza):
    for xpath in xpaths:
        expected = True
        real_xpath = xpath
        # We can check that a stanza DOESN’T match, by adding a ! before it.
        if xpath.startswith('!'):
            expected = False
            xpath = xpath[1:]
        matched = match(stanza, xpath)
        if (expected and not matched) or (not expected and matched):
            raise StanzaError("Received stanza\n%s\ndid not match expected xpath\n%s" % (stanza, real_xpath))
    if after:
        if isinstance(after, collections.Iterable):
            for af in after:
                af(stanza, xmpp)
        else:
            after(stanza, xmpp)

def check_xpath_optional(xpaths, xmpp, after, stanza):
    try:
        check_xpath(xpaths, xmpp, after, stanza)
    except StanzaError:
        raise SkipStepError()

def all_xpaths_match(stanza, xpaths):
    try:
        check_xpath(xpaths, None, None, stanza)
    except StanzaError:
        return False
    return True

def check_list_of_xpath(list_of_xpaths, xmpp, stanza):
    found = False
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

def extract_attribute(xpath, name):
    def f(xpath, name, stanza):
        matched = match(stanza, xpath)
        return matched[0].get(name)
    return partial(f, xpath, name)

def extract_text(xpath, stanza):
    matched = match(stanza, xpath)
    return matched[0].text

def save_value(name, func):
    def f(name, func, stanza, xmpp):
        xmpp.saved_values[name] = func(stanza)
    return partial(f, name, func)

def expect_stanza(*args, optional=False, after=None):
    def f(*xpaths, xmpp, biboumi, optional, after):
        replacements = common_replacements
        replacements.update(xmpp.saved_values)
        check_func = check_xpath if not optional else check_xpath_optional
        xmpp.stanza_checker = partial(check_func, [xpath.format_map(replacements) for xpath in xpaths], xmpp, after)
    return partial(f, *args, optional=optional, after=after)

def send_stanza(stanza):
    def internal(stanza, xmpp, biboumi):
        replacements = common_replacements
        replacements.update(xmpp.saved_values)
        xmpp.send_raw(stanza.format_map(replacements))
        asyncio.get_event_loop().call_soon(xmpp.run_scenario)
    return partial(internal, stanza)

def expect_unordered(*args):
    def f(*lists_of_xpaths, xmpp, biboumi):
        formatted_list_of_xpaths = []
        for list_of_xpaths in lists_of_xpaths:
            formatted_xpaths = []
            for xpath in list_of_xpaths:
                formatted_xpath = xpath.format_map(common_replacements)
                formatted_xpaths.append(formatted_xpath)
            formatted_list_of_xpaths.append(tuple(formatted_xpaths))
        expect_unordered_already_formatted(formatted_list_of_xpaths, xmpp, biboumi)
    return partial(f, *args)

def expect_unordered_already_formatted(formatted_list_of_xpaths, xmpp, biboumi):
    xmpp.stanza_checker = partial(check_list_of_xpath, formatted_list_of_xpaths, xmpp)

def sleep_for(duration):
    def f(duration, xmpp, biboumi):
        time.sleep(duration)
        asyncio.get_event_loop().call_soon(xmpp.run_scenario)
    return partial(f, duration)

def save_current_timestamp_plus_delta(key, delta):
    def f(key, delta, message, xmpp):
        now_plus_delta = datetime.datetime.utcnow() + delta
        xmpp.saved_values[key] = now_plus_delta.strftime("%FT%T.967Z")
    return partial(f, key, delta)
