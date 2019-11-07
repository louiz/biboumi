########################
Developper documentation
########################

End-to-end test suite
---------------------

A powerful test suite has been developped to test biboumi’s behaviour in
many scenarios. Its goal is to simulate a real-world usage of biboumi,
including its interactions with a real IRC server an a real XMPP client.

An IRC server is started, with a specific version and configuration, then,
for every scenario that we want to test:

- Biboumi is started, with a specific configuration
- An XMPP “client” starts, communicates with biboumi and checks that
  biboumi responds in the expected way.

The XMPP client is actually not a real client, it’s a python script that
uses the slixmpp library to imitate an XMPP server that would transmit the
stanzas of one client to its component (biboumi). In real life, the
communication to biboumi is done between the XMPP server and biboumi, but
since the server just forwards the messages that clients send unmodified,
we’ll call that “the clients”.

A scenario is a list of functions that will be executed one by one, to
verify the behaviour of one specific feature. Ideally, they should be
short and test one specific aspect.

Available functions
~~~~~~~~~~~~~~~~~~~

.. py:function:: send_stanza(str)

  sends one stanza to biboumi. The stanza is written entirely
  as a string (with a few automatic replacements). The “from” and “to”
  values have to be specified everytime, because each stanza can come from
  different clients and be directed to any IRC server/channel

  .. code-block:: python

    send_stanza("<message from='{jid_one}/{resource_one}' to='#foo@{biboumi_host}' type='groupchat'><body>coucou</body></message>"),

.. py:function:: expect_stanza(xpath[, …])

  Waits for a stanza to be received by biboumi, and checks that this
  stanza matches one or more xpath. If the stanza doesn’t match all the
  given xpaths, then the scenario ends and we report that as an error.

  .. code-block:: python

    expect_stanza("/message[@from='#foo@{biboumi_host}/{nick_one}']/body[text()='coucou']",
                  "/message/delay:delay[@from='#foo@{biboumi_host}']"),

  This waits for exactly 1 stanza, that is compared against 2 xpaths. Here
  we check that it is a message, that it has the proper `from` value, the
  correct body, and a <delay/>.

.. py:function:: expect_unordered(list_of_xpaths[, list_of_xpaths, …])

  we wait for more than one stanzas, that could be received in any order.
  For example, in certain scenario, we wait for two presence stanzas, but
  it’s perfectly valid to receive them in any order (one is for one
  client, the other one for an other client). To do that, we pass multiple
  lists of xpath. Each list can contain one or more xpath (just like
  `expect_stanza`). When a stanza is received, it is compared with all the
  xpaths of the first list. If it doesn’t match, it is compared with the
  xpaths of the second list, and so on. If nothing matchs, it’s an error
  and we stop this scenario. If the stanza matches with one of the xpath
  lists, we remove that list, and we wait for the next stanza, until there
  are no more xpaths.

  .. code-block:: python

    expect_unordered(
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_two}/{resource_one}'][@type='unavailable']/muc_user:x/muc_user:item[@nick='Bernard']",
                  "/presence/muc_user:x/muc_user:status[@code='303']",
             ],
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_two}/{resource_one}']",
             ],
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_one}'][@to='{jid_one}/{resource_one}'][@type='unavailable']/muc_user:x/muc_user:item[@nick='Bernard']",
                  "/presence/muc_user:x/muc_user:status[@code='303']",
                  "/presence/muc_user:x/muc_user:status[@code='110']",
             ],
             [
                  "/presence[@from='#foo%{irc_server_one}/{nick_three}'][@to='{jid_one}/{resource_one}']",
                  "/presence/muc_user:x/muc_user:status[@code='110']",
             ],
      ),

  This will wait for 4 stanzas that could be received in any order.

To avoid many repetitions between each tests, some helpful sequences are
available, `sequences.connection(…)` and `sequences.connection_tls(…)`.
They do all the steps that are needed (send and receive stanzas) to
connect to the component, or an IRC server.

It’s also possible to reuse one simple scenario into an other scenario.
The most notable example is to start your own scenario with
`scenarios.simple_channel_join.scenario`, if you need your client to be in
a channel before you can start your actual scenario. For example if you
want to test the behaviour of a topic change, you need to first join a
channel. Since this is a very common patern, it’s simpler to just included
this very basic scenario at the start of your own scenarios, instead of
copy pasting the same thing over and over.

Examples of a scenario
~~~~~~~~~~~~~~~~~~~~~~

First example
^^^^^^^^^^^^^

Here we’ll describe how to write your own scenario, from scratch. For this, we will take an existing scenario and explain how it was written, line by line.

See for example the scenario tests/end_to_end/scenarios/self_ping_on_real_channel.py

.. code-block:: python

  from scenarios import *

All the tests should start with this import. It imports the file
tests/end_to_end/scenarios/__init__.py This make all the functions
available (send_stanza, expect_stanza…) available, as well as some very
common scenarios that you often need to re-use.

.. code-block:: python

  scenario = (
    # …
  )

This is the only required element of your scenario. This object is a tuple of funcion calls OR other scenarios.

.. code-block:: python

  scenarios.simple_channel_join.scenario,

The first line of our scenario is actually including an other existing
scenario. You can find it at
tests/end_to_end/scenarios/simple_channel_join.py As its name shows, it’s
very basic: one client {jid_one}/{resource_one} just joins one room
#foo%{irc_server_one} with the nick {nick_one}.

Since we want to test the behaviour of a ping to ourself when we are in a
room, we just join this room without repeating everything.

It is possible to directly insert a scenario inside our scenario without
having to extract all the steps: the test suite is smart enough to detect
that and extract the inner steps automatically.

.. code-block:: python

  # Send a ping to ourself
  send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='first_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
  expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='first_ping']"),

Here we simple send an iq stanza, properly formatted, using the same JIDs
{jid_one}/{resource_one} and #foo%{irc_server_one}/{nick_one} to ping
ourself in the room. We them immediately expect one stanza to be received,
that is the response to our ping. It only contains one single xpath
because everything we need to check can be expressed in one line.

Note that it is recommended to explain all the steps of your scenario with
comments. This helps understand what is being tested, and why, without
having to analyze all the stanza individually.

.. code-block:: python

  # Now join the same room, from the same bare JID, behind the same nick
  send_stanza("<presence from='{jid_one}/{resource_two}' to='#foo%{irc_server_one}/{nick_one}' />"),
  expect_stanza("/presence[@to='{jid_one}/{resource_two}'][@from='#foo%{irc_server_one}/{nick_one}']/muc_user:x/muc_user:item[@affiliation='admin'][@role='moderator']",
                "/presence/muc_user:x/muc_user:status[@code='110']"),

  expect_stanza("/message[@from='#foo%{irc_server_one}'][@type='groupchat'][@to='{jid_one}/{resource_two}']/subject[not(text())]"),

Here we send a presence stanza to join the same channel with an other
resource (note the {resource_two}). As a result, we expect two stanzas:
The first stanza (our self-presence) is checked against two xpaths, and
the second stanza (the empty subject of the room) against only one.

.. code-block:: python

  # And re-send a self ping
  send_stanza("<iq type='get' from='{jid_one}/{resource_one}' id='second_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
  expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_one}'][@id='second_ping']"),
  ## And re-do exactly the same thing, just change the resource initiating the self ping
  send_stanza("<iq type='get' from='{jid_one}/{resource_two}' id='third_ping' to='#foo%{irc_server_one}/{nick_one}'><ping xmlns='urn:xmpp:ping' /></iq>"),
  expect_stanza("/iq[@from='#foo%{irc_server_one}/{nick_one}'][@type='result'][@to='{jid_one}/{resource_two}'][@id='third_ping']"),

And finally, we test a second ping, and check that the behaviour is correct that we now have two resources in that channel.

Second example
^^^^^^^^^^^^^^

Sometimes we want to do more with the received stanzas. For example we
need to extract some values from the received stanzas, to reuse them in
future stanzas we send. The most obvious example is iq IDs, that we need
to extract, to reuse them in our response.

Let’s use for example the tests/end_to_end/scenarios/execute_incomplete_hello_adhoc_command.py scenario:

.. code-block:: python

  from scenarios import *

  scenario = (
    send_stanza("<iq type='set' id='hello-command1' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='hello' action='execute' /></iq>"),
    expect_stanza("/iq[@type='result']/commands:command[@node='hello'][@sessionid][@status='executing']",
                  "/iq/commands:command/commands:actions/commands:complete",
                  after = save_value("sessionid", extract_attribute("/iq[@type='result']/commands:command[@node='hello']", "sessionid"))
                  ),

Here is where the magic happens: as an additional argument to the
expect_stanza function, we pass an other function (callback) with the
“after=” keyword argument. This “after” callback gets called once the
expected stanza has been received and validated. Here we use
`save_value(key, value)`. This function just saves a value in our global
values that can be used with “send_stanza”, associated with the given
“key”. For example if you do `save_value("something_important", "blah")`
then you can use `{something_important}` in any future stanza that you
send and it will be replaced with “blah”.

But this is only useful if we can save some value that we extract from the
stanza. That’s where `extract_attribute(xpath, attribute_name)` comes into
play. As the first argument, you pass an xpath corresponding to one
specific node of the XML that is received, and the second argument is just
the name of the attribute whose value you want.

Here, we extract the value of the “sessionid=” in the node `<iq
type='result'><commands:command node='hello' sessionid='…' /></iq>`, and
we save that value, globally, with the name “sessionid”.

.. code-block:: python

  send_stanza("<iq type='set' id='hello-command2' from='{jid_one}/{resource_one}' to='{biboumi_host}'><command xmlns='http://jabber.org/protocol/commands' node='hello' sessionid='{sessionid}' action='complete'><x xmlns='jabber:x:data' type='submit'></x></command></iq>"),

Here we send a second iq, to continue our ad-hoc command, and we use {sessionid} to indicate that we are continuing the session we started before.
