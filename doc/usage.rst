Quick-start
-----------

When a user joins an IRC channel on an IRC server (see `Join an IRC
channel`_), biboumi connects to the remote IRC server, sets the user’s nick
as requested, and then tries to join the specified channel.  If the same
user subsequently tries to connect to an other channel on the same server,
the same IRC connection is used.  If, however, an other user wants to join
an IRC channel on that same IRC server, biboumi opens a new connection to
that server.  Biboumi connects once to each IRC servner, for each user on it.

Additionally, if one user is using more than one clients (with the same bare
JID), they can join the same IRC channel (on the same server) behind one
single nickname.  Biboumi will forward all the messages (the channel ones and
the private ones) and the presences to all the resources behind that nick.
There is no need to have multiple nicknames and multiple connections to be
able to take part in a conversation (or idle) in a channel from a mobile client
while the desktop client is still connected, for example.

To cleanly shutdown the component, send a SIGINT or SIGTERM signal to it.
It will send messages to all connected IRC and XMPP servers to indicate a
reason why the users are being disconnected.  Biboumi exits when the end of
communication is acknowledged by all IRC servers.  If one or more IRC
servers do not respond, biboumi will only exit if it receives the same
signal again or if a 2 seconds delay has passed.

.. note:: If you use a biboumi that you have no control on: remember that the
 administrator of the gateway you use is able to view all your IRC
 conversations, whether you’re using encryption or not.  This is exactly as
 if you were running your IRC client on someone else’s server. Only use
 biboumi if you trust its administrator (or, better, if you are the
 administrator) or if you don’t intend to have any private conversation.

Addressing
----------

IRC entities are represented by XMPP JIDs.  The domain part of the JID is
the domain served by biboumi (the part after the `@`, biboumi.example.com in
the examples), and the local part (the part before the `@`) depends on the
concerned entity.

IRC channels and IRC users have a local part formed like this:
``name`` % ``irc_server``.

``name`` can be a channel name or an user nickname. The distinction between
the two is based on the first character: by default, if the name starts with
``'#'`` or ``'&'`` (but this can be overridden by the server, using the
ISUPPORT extension) then it’s a channel name, otherwise this is a nickname.

There is two ways to address an IRC user, using a local part like this:
``nickname`` % ``irc_server`` or by using the in-room address of the
participant, like this:
``channel_name`` % ``irc_server`` @ ``biboumi.example.com`` / ``Nickname``

The second JID is available only to be compatible with XMPP clients when the
user wants to send a private message to the participant ``Nickname`` in the
room ``channel_name%irc_server@biboumi.example.com``.

On XMPP, the node part of the JID can only be lowercase.  On the other hand,
IRC nicknames are case-insensitive, this means that the nicknames toto,
Toto, tOtO and TOTO all represent the same IRC user.  This means you can
talk to the user toto, and this will work.

Also note that some IRC nicknames or channels may contain characters that are
not allowed in the local part of a JID (for example '@').  If you need to send a
message to a nick containing such a character, you can use a jid like
``%irc.example.com@biboumi.example.com/AnnoyingNickn@me``, because the JID
``AnnoyingNickn@me%irc.example.com@biboumi.example.com`` would not work.
And if you need to address a channel that contains such invalid characters, you
have to use `jid-escaping <http://www.xmpp.org/extensions/xep-0106.html#escaping>`_,
and replace each of these characters with their escaped version, for example to
join the channel ``#b@byfoot``, you need to use the following JID:
``#b\40byfoot%irc.example.com@biboumi.example.com``.


Examples:

* ``#foo%irc.example.com@biboumi.example.com`` is the #foo IRC channel, on the
  irc.example.com IRC server, and this is served by the biboumi instance on
  biboumi.example.com

* ``toto%irc.example.com@biboumi.example.com`` is the IRC user named toto, or
  TotO, etc.

* ``irc.example.com@biboumi.example.com`` is the IRC server irc.example.com.

Note: Some JIDs are valid but make no sense in the context of
biboumi:

* ``#test%@biboumi.example.com``, or any other JID that does not contain an
  IRC server is invalid. Any message to that kind of JID will trigger an
  error, or will be ignored.

If compiled with Libidn, an IRC channel participant has a bare JID
representing the “hostname” provided by the IRC server.  This JID can only
be used to set IRC modes (for example to ban a user based on its IP), or to
identify user. It cannot be used to contact that user using biboumi.

Join an IRC channel
-------------------

To join an IRC channel ``#foo`` on the IRC server ``irc.example.com``,
join the XMPP MUC ``#foo%irc.example.com@biboumi.example.com``.

Connect to an IRC server
------------------------

The connection to the IRC server is automatically made when the user tries
to join any channel on that IRC server.  The connection is closed whenever
the last channel on that server is left by the user.

Roster
------

You can add some JIDs provided by biboumi into your own roster, to receive
presence from them. Biboumi will always automatically accept your requests.

Biboumi’s JID
~~~~~~~~~~~~~

By adding the component JID into your roster, the user will receive an available
presence whenever it is started, and an unavailable presence whenever it is being
shutdown.  This is useful to quickly view if that biboumi instance is started or
not.

IRC server JID
~~~~~~~~~~~~~~

These presence will appear online in the user’s roster whenever they are
connected to that IRC server (see `Connect to an IRC server`_ for more
details). This is useful to keep track of which server an user is connected
to: this is sometimes hard to remember, when they have many clients, or if
they are using persistent channels.

Channel messages
----------------

On XMPP, unlike on IRC, the displayed order of the messages is the same for
all participants of a MUC.  Biboumi can not however provide this feature, as
it cannot know whether the IRC server has received and forwarded the
messages to other users.  This means that the order of the messages
displayed in your XMPP client may not be the same as the order on other
IRC users’.

History
-------

Public channel messages are saved into archives, inside the database,
unless the `record_history` option is set to false by that user (see
`Ad-hoc commands`_). Private messages (messages that are sent directly to
a nickname, not a channel) are never stored in the database.

A channel history can be retrieved by using `Message archive management
(MAM) <https://xmpp.org/extensions/xep-0313.htm>`_ on the channel JID.
The results can be filtered by start and end dates.

When a channel is joined, if the client doesn’t specify any limit, biboumi
sends the `max_history_length` last messages found in the database as the
MUC history.  If a client wants to only use MAM for the archives (because
it’s more convenient and powerful), it should request to receive no
history by using an attribute maxchars='0' or maxstanzas='0' as defined in
XEP 0045, and do a proper MAM request instead.

Note: the maxchars attribute is ignored unless its value is exactly 0.
Supporting it properly would be very hard and would introduce a lot of
complexity for almost no benefit.

For a given channel, each user has her or his own archive.  The content of
the archives are never shared, and thus a user can not use someone else’s
archive to get the messages that they didn’t receive when they were
offline. Although this feature would be very convenient, this would
introduce a very important privacy issue: for example if a biboumi gateway
is used by two users, by querying the archive one user would be able to
know whether or not the other user was in a room at a given time.


List channels
-------------

You can list the IRC channels on a given IRC server by sending an XMPP
disco items request on the IRC server JID.  The number of channels on some
servers is huge so the result stanza may be very big, unless your client
supports result set management (XEP 0059)

Nicknames
---------

On IRC, nicknames are server-wide.  This means that one user only has one
single nickname at one given time on all the channels of a server. This is
different from XMPP where a user can have a different nick on each MUC,
even if these MUCs are on the same server.

This means that the nick you choose when joining your first IRC channel on
a given IRC server will be your nickname in all other channels that you
join on that same IRC server.

If you explicitely change your nickname on one channel, your nickname will
be changed on all channels on the same server as well. Joining a new
channel with a different nick, however, will not change your nick.  The
provided nick will be ignored, in order to avoid changing your nick on the
whole server by mistake.  If you want to have a different nickname in the
channel you’re going to join, you need to do it explicitly with the NICK
command before joining the channel.

Private messages
----------------

Private messages are handled differently on IRC and on XMPP.  On IRC, you
talk directly to one server-user: toto on the channel #foo is the same user
as toto on the channel #bar (as long as these two channels are on the same
IRC server).  By default you will receive private messages from the “global”
user (aka nickname%irc.example.com@biboumi.example.com), unless you
previously sent a message to an in-room participant (something like
\#test%irc.example.com@biboumi.example.com/nickname), in which case future
messages from that same user will be received from that same “in-room” JID.

Notices
-------

Notices are received exactly like private messages.  It is not possible to
send a notice.

Topic
-----

The topic can be set and retrieved seemlessly. The unique difference is that
if an XMPP user tries to set a multiline topic, every line return (\\n) will
be replaced by a space, because the IRC server wouldn’t accept it.

Invitations
-----------

If the invited JID is a user JID served by this biboumi instance, it will forward the
invitation to the target nick, over IRC.
Otherwise, the mediated instance will directly be sent to the invited JID, over XMPP.

Example: if the user wishes to invite the IRC user “FooBar” into a room, they can
invite one of the following “JIDs” (one of them is not a JID, actually):

- foobar%anything@biboumi.example.com
- anything@biboumi.example.com/FooBar
- FooBar

(Note that the “anything” parts are simply ignored because they carry no
additional meaning for biboumi: we already know which IRC server is targeted
using the JID of the target channel.)

Otherwise, any valid JID can be used, to invite any XMPP user.

Kicks and bans
--------------

Kicks are transparently translated from one protocol to another.  However
banning an XMPP participant has no effect.  To ban an user you need to set a
mode +b on that user nick or host (see `IRC modes`_) and then kick it.

Encoding
--------

On XMPP, the encoding is always ``UTF-8``, whereas on IRC the encoding of
each message can be anything.

This means that biboumi has to convert everything coming from IRC into UTF-8
without knowing the encoding of the received messages.  To do so, it checks
if each message is UTF-8 valid, if not it tries to convert from
``iso_8859-1`` (because this appears to be the most common case, at least
on the channels I visit) to ``UTF-8``.  If that conversion fails at some
point, a placeholder character ``'�'`` is inserted to indicate this
decoding error.

Messages are always sent in UTF-8 over IRC, no conversion is done in that
direction.

IRC modes
---------

One feature that doesn’t exist on XMPP but does on IRC is the ``modes``.
Although some of these modes have a correspondance in the XMPP world (for
example the ``+o`` mode on a user corresponds to the ``moderator`` role in
XMPP), it is impossible to map all these modes to an XMPP feature.  To
circumvent this problem, biboumi provides a raw notification when modes are
changed, and lets the user change the modes directly.

To change modes, simply send a message starting with “``/mode``” followed by
the modes and the arguments you want to send to the IRC server.  For example
“/mode +aho louiz”.  Note that your XMPP client may interprete messages
begining with “/” like a command.  To actually send a message starting with
a slash, you may need to start your message with “//mode” or “/say /mode”,
depending on your client.

When a mode is changed, the user is notified by a message coming from the
MUC bare JID, looking like “Mode #foo [+ov] [toto tutu]”.  In addition, if
the mode change can be translated to an XMPP feature, the user will be
notified of this XMPP event as well. For example if a mode “+o toto” is
received, then toto’s role will be changed to moderator.  The mapping
between IRC modes and XMPP features is as follow:

``+q``
  Sets the participant’s role to ``moderator`` and its affiliation to ``owner``.

``+a``
  Sets the participant’s role to ``moderator`` and its affiliation to ``owner``.

``+o``
  Sets the participant’s role to ``moderator`` and its affiliation to  ``admin``.

``+h``
  Sets the participant’s role to ``moderator`` and its affiliation to  ``member``.

``+v``
  Sets the participant’s role to ``participant`` and its affiliation to ``member``.

Similarly, when a biboumi user changes some participant's affiliation or role, biboumi translates that in an IRC mode change.

Affiliation set to ``none``
  Sets mode to -vhoaq

Affiliation set to ``member``
  Sets mode to +v-hoaq

Role set to ``moderator``
  Sets mode to +h-oaq

Affiliation set to ``admin``
  Sets mode to +o-aq

Affiliation set to ``owner``
  Sets mode to +a-q

Ad-hoc commands
---------------

Biboumi supports a few ad-hoc commands, as described in the XEP 0050.
Different ad-hoc commands are available for each JID type.

On the gateway itself
~~~~~~~~~~~~~~~~~~~~~

.. note:: For example on the JID biboumi.example.com

ping
^^^^
Just respond “pong”

hello
^^^^^

Provide a form, where the user enters their name, and biboumi responds
with a nice greeting.

disconnect-user
^^^^^^^^^^^^^^^

Only available to the administrator. The user provides a list of JIDs, and
a quit message. All the selected users are disconnected from all the IRC
servers to which they were connected, using the provided quit message.
Sending SIGINT to biboumi is equivalent to using this command by selecting
all the connected JIDs and using the “Gateway shutdown” quit message,
except that biboumi does not exit when using this ad-hoc command.

disconnect-from-irc-servers
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Disconnect a single user from one or more IRC server.  The user is
immediately disconnected by closing the socket, no message is sent to the
IRC server, but the user is of course notified with an XMPP message.  The
administrator can disconnect any user, while the other users can only
disconnect themselves.

configure
^^^^^^^^^

Lets each user configure some options that applies globally.
The provided configuration form contains these fields:

- **Record History**: whether or not history messages should be saved in
  the database.
- **Max history length**: The maximum number of lines in the history that
  the server is allowed to send when joining a channel.
- **Persistent**: Overrides the value specified in each individual
  channel. If this option is set to true, all channels are persistent,
  whether or not their specific value is true or false. This option is true
  by default for everyone if the `persistent_by_default` configuration
  option is true, otherwise it’s false. See below for more details on what a
  persistent channel is. This value is

On a server JID
~~~~~~~~~~~~~~~

 E.g on the JID chat.freenode.org@biboumi.example.com

- **configure**: Lets each user configure some options that applies to the
  concerned IRC server.  The provided configuration form contains these
  fields:

    - **Address**: This address (IPv4, IPv6 or hostname) will be used, when
      biboumi connects to this server. This is a very handy way to have a
      custom name for a network, and be able to edit the address to use
      if one endpoint for that server is dead, but continue using the same
      JID. For example, a user could configure the server
      “freenode@biboumi.example.com”, set “chat.freenode.net” in its
      “Address” field, and then they would be able to use “freenode” as
      the network name forever: if “chat.freenode.net” breaks for some
      reason, it can be changed to “irc.freenode.org” instead, and the user
      would not need to change all their bookmarks and settings.
    - **Realname**: The customized “real name” as it will appear on the
      user’s whois. This option is not available if biboumi is configured
      with realname_customization to false.
    - **Username**: The “user” part in your `user@host`. This option is not
      available if biboumi is configured with realname_customization to
      false.
    - **In encoding**: The incoming encoding. Any received message that is not
      proper UTF-8 will be converted will be converted from the configured
      In encoding into UTF-8. If the conversion fails at some point, some
      characters will be replaced by the placeholders.
    - **Out encoding**: Currently ignored.
    - **After-connection IRC commands**: Raw IRC commands that will be sent
      one by one to the server immediately after the connection has been
      successful. It can for example be used to identify yourself using
      NickServ, with a command like this: `PRIVMSG NickServ :identify
      PASSWORD`.
    - **Ports**: The list of TCP ports to use when connecting to this IRC server.
      This list will be tried in sequence, until the connection succeeds for
      one of them. The connection made on these ports will not use TLS, the
      communication will be insecure. The default list contains 6697 and 6670.
    - **TLS ports**: A second list of ports to try when connecting to the IRC
      server. The only difference is that TLS will be used if the connection
      is established on one of these ports. All the ports in this list will
      be tried before using the other plain-text ports list. To entirely
      disable any non-TLS connection, just remove all the values from the
      “normal” ports list. The default list contains 6697.
    - **Verify certificate**: If set to true (the default value), when connecting
      on a TLS port, the connection will be aborted if the certificate is
      not valid (for example if it’s not signed by a known authority, or if
      the domain name doesn’t match, etc). Set it to false if you want to
      connect on a server with a self-signed certificate.
    - **SHA-1 fingerprint of the TLS certificate to trust**: if you know the hash
      of the certificate that the server is supposed to use, and you only want
      to accept this one, set its SHA-1 hash in this field.
    - **Nickname**: A nickname that will be used instead of the nickname provided
      in the initial presence sent to join a channel. This can be used if the
      user always wants to have the same nickname on a given server, and not
      have to bother with setting that nick in all the bookmarks on that
      server. The nickname can still manually be changed with a standard nick
      change presence.
    - **Server password**: A password that will be sent just after the connection,
      in a PASS command. This is usually used in private servers, where you’re
      only allowed to connect if you have the password. Note that, although
      this is NOT a password that will be sent to NickServ (or some author
      authentication service), some server (notably Freenode) use it as if it
      was sent to NickServ to identify your nickname.
    - **Throttle limit**: specifies a number of messages that can be sent
      without a limit, before the throttling takes place. When messages
      are throttled, only one command per second is sent to the server.
      The default is 10. You can lower this value if you are ever kicked
      for excess flood. If the value is 0, all messages are throttled. To
      disable this feature, set it to a negative number, or an empty string.

- **get-irc-connection-info**: Returns some information about the IRC server,
  for the executing user. It lets the user know if they are connected to
  this server, from what port, with or without TLS, and it gives the list
  of joined IRC channel, with a detailed list of which resource is in which
  channel.

On a channel JID
~~~~~~~~~~~~~~~~

E.g on the JID #test%chat.freenode.org@biboumi.example.com

- **configure**: Lets each user configure some options that applies to the
  concerned IRC channel.  Some of these options, if not configured for a
  specific channel, defaults to the value configured at the IRC server
  level.  For example the encoding can be specified for both the channel
  and the server.  If an encoding is not specified for a channel, the
  encoding configured in the server applies. The provided configuration
  form contains these fields:
  - **In encoding**: see the option with the same name in the server configuration
  form.
  - **Out encoding**: Currently ignored.
  - **Persistent**: If set to true, biboumi will stay in this channel even when
  all the XMPP resources have left the room. I.e. it will not send a PART
  command, and will stay idle in the channel until the connection is
  forcibly closed. If a resource comes back in the room again, and if
  the archiving of messages is enabled for this room, the client will
  receive the messages that where sent in this channel. This option can be
  used to make biboumi act as an IRC bouncer.
  - **Record History**: whether or not history messages should be saved in
  the database, for this specific channel. If the value is “unset” (the
  default), then the value configured globally is used. This option is there,
  for example, to be able to enable history recording globally while disabling
  it for a few specific “private” channels.

Raw IRC messages
----------------

Biboumi tries to support as many IRC features as possible, but doesn’t
handle everything yet (or ever).  In order to let the user send any
arbitrary IRC message, biboumi forwards any XMPP message received on an IRC
Server JID (see `Addressing`_) as a raw command to that IRC server.

For example, to WHOIS the user Foo on the server irc.example.com, a user can
send the message “WHOIS Foo” to ``irc.example.com@biboumi.example.com``.

The message will be forwarded as is, without any modification appart from
adding ``\r\n`` at the end (to make it a valid IRC message).  You need to
have a little bit of understanding of the IRC protocol to use this feature.
