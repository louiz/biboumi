BIBOUMI 1 "2014-06-02"
======================

NAME
----

biboumi - XMPP gateway to IRC

DESCRIPTION
-----------

Biboumi is an XMPP gateway that connects to IRC servers and translates
between the two protocols. It can be used to access IRC channels using any
XMPP client as if these channels were XMPP MUCs.

SYNOPSIS
--------

`biboumi` [`config_filename`]

OPTIONS
-------

Available command line options:

`config_filename`

  Specify the file to read for configuration. See *CONFIG* section for more
  details on its content.

CONFIG
------

The configuration file uses a simple format of the form
`"option=value"`. Here is a description of each possible option:

`hostname` (mandatory)

  The hostname served by the XMPP gateway.  This domain must be configured in
  the XMPP server as an external component.  See the manual for your XMPP
  server for more information.
  For prosody, see http://prosody.im/doc/components#adding_an_external_component

`password` (mandatory)

  The password used to authenticate the XMPP component to your XMPP server.
  This password must be configured in the XMPP server, associated with the
  external component on *hostname*.

`port`

  The TCP port to use to connect to the local XMPP component. The default
  value is 5347.

`admin`

  The bare JID of the gateway administrator. This JID will have more
  privileges than other standard users (the admin thus needs to check their
  privileges), for example some administration ad-hoc commands will only be
  available to that JID.

`log_file`

  A filename into which logs are written.  If none is provided, the logs are
  written on standard output.

`log_level`

  Indicate what type of log messages to write in the logs.  Value can be
  from 0 to 3.  0 is debug, 1 is info, 2 is warning, 3 is error.  The
  default is 0, but a more practical value for production use is 1.

The configuration can be re-read at runtime (you can for example change the
log level without having to restart biboumi) by sending SIGUSR1 or SIGUSR2
(see kill(1)) to the process.

USAGE
-----

Biboumi acts as a server, it should be run as a daemon that lives in the
background for as long as it is needed.  Note that biboumi does not
daemonize itself, this task should be done by your init system (SysVinit,
systemd, upstart).

When started, biboumi connects, without encryption (see *SECURITY*), to the
local XMPP server on the port `5347` and authenticates with the provided
password.  Biboumi then serves the configured `hostname`: this means that
all XMPP stanza with a `to` JID on that domain will be forwarded to biboumi
by the XMPP server, and biboumi will only send messages coming from that
hostname.

When a user joins an IRC channel on an IRC server (see *Join an IRC
channel*), biboumi connects to the remote IRC server, sets the user’s nick
as requested, and then tries to join the specified channel.  If the same
user subsequently tries to connect to an other channel on the same server,
the same IRC connection is used.  If, however, an other user wants to join
an IRC channel on that same IRC server, biboumi opens a new connection to
that server.  Biboumi connects once to each IRC server, for each user on it.

To cleanly shutdown the component, send a SIGINT or SIGTERM signal to it.
It will send messages to all connected IRC and XMPP servers to indicate a
reason why the users are being disconnected.  Biboumi exits when the end of
communication is acknowledged by all IRC servers.  If one or more IRC
servers do not respond, biboumi will only exit if it receives the same
signal again or if a 2 seconds delay has passed.

### Addressing

IRC entities are represented by XMPP JIDs.  The domain part of the JID is
the domain served by biboumi (the part after the `@`, biboumi.example.com in
the examples), and the local part (the part before the `@`) depends on the
concerned entity.

IRC channels have a local part formed like this:
`channel_name`%`irc_server`.

If the IRC channel you want to adress starts with the `'#'` character (or an
other character, announced by the IRC server, like `'&'`, `'+'` or `'!'`),
then you must include it in the JID.  Some other gateway implementations, as
well as some IRC clients, do not require them to be started by one of these
characters, adding an implicit `'#'` in that case.  Biboumi does not do that
because this gets confusing when trying to understand the difference between
the channels *#foo*, and *##foo*. Note that biboumi does not use the
presence of these special characters to identify an IRC channel, only the
presence of the separator `%` is used for that.

The channel name can also be empty (for example `%irc.example.com`), in that
case this represents the virtual channel provided by biboumi.  See *Connect
to an IRC server* for more details.

There is two ways to address an IRC user, using a local part like this:
`nickname`!`irc_server`
or by using the in-room address of the participant, like this:
`channel_name`%`irc_server`@`biboumi.example.com`/`Nickname`

The second JID is available only to be compatible with XMPP clients when the
user wants to send a private message to the participant `Nickname` in the
room `channel_name%irc_server@biboumi.example.com`.

On XMPP, the node part of the JID can only be lowercase.  On the other hand,
IRC nicknames are case-insensitive, this means that the nicknames toto,
Toto, tOtO and TOTO all represent the same IRC user.  This means you can
talk to the user toto, and this will work.

Examples:

  `#foo%irc.example.com@biboumi.example.com` is the #foo IRC channel, on the
  irc.example.com IRC server, and this is served by the biboumi instance on
  biboumi.example.com

  `toto!irc.example.com@biboumi.example.com` is the IRC user named toto, or
  TotO, etc.

  `irc.example.com@biboumi.example.com` is the IRC server irc.example.com.

  `%irc.example.com@biboumi.example.com` is the virtual channel provided by
  biboumi, for the IRC server irc.example.com.

Note: Some JIDs are valid but make no sense in the context of
biboumi:

  `!irc.example.com@biboumi.example.com` is the empty-string nick on the
  irc.example.com server. It makes no sense to try to send messages to it.

  `#test%@biboumi.example.com`, or any other JID that does not contain an
  IRC server is invalid. Any message to that kind of JID will trigger an
  error, or will be ignored.

If compiled with Libidn, an IRC channel participant has a bare JID
representing the “hostname” provided by the IRC server.  This JID can only
be used to set IRC modes (for example to ban a user based on its IP), or to
identify user. It cannot be used to contact that user using biboumi.

### Join an IRC channel

To join an IRC channel `#foo` on the IRC server `irc.example.com`,
join the XMPP MUC `#foo%irc.example.com@hostname`.

### Connect to an IRC server

The connection to the IRC server is automatically made when the user tries
to join any channel on that IRC server.  The connection is closed whenever
the last channel on that server is left by the user.  To be able to stay
connected to an IRC server without having to be in a real IRC channel,
biboumi provides a virtual channel on the jid
`%irc.example.com@biboumi.example.com`.  For example if you want to join the
channel `#foo` on the server `irc.example.com`, but you need to authenticate
to a bot of the server before you’re allowed to join it, you can first join
the room `%irc.example.com@biboumi.example.com` (this will effectively
connect you to the IRC server without joining any room), then send your
authentication message to the user `bot!irc.example.com@biboumi.example.com`
and finally join the room `#foo%irc.example.com@biboumi.example.com`.

### Channel messages

On XMPP, unlike on IRC, the displayed order of the messages is the same for
all participants of a MUC.  Biboumi can not however provide this feature, as
it cannot know whether the IRC server has received and forwarded the
messages to other users.  This means that the order of the messages
displayed in your XMPP client may not be the same than the order on other
IRC users’.

### Nicknames

On IRC, nicknames are server-wide.  This means that one user only has one
single nickname at one given time on all the channels of a server. This is
different from XMPP where a user can have a different nick on each MUC,
even if these MUCs are on the same server.

This means that the nick you choose when joining your first IRC channel on a
given IRC server will be your nickname in all other channels that you join
on that same IRC server.
If you explicitely change your nickname on one channel, your nickname will
be changed on all channels on the same server as well.

### Private messages

Private messages are handled differently on IRC and on XMPP.  On IRC, you
talk directly to one server-user: toto on the channel #foo is the same user
as toto on the channel #bar (as long as these two channels are on the same
IRC server).  By default you will receive private messages from the “global”
user (aka nickname!irc.example.com@biboumi.example.com), unless you
previously sent a message to an in-room participant (something like
\#test%irc.example.com@biboumi.example.com/nickname), in which case future
messages from that same user will be received from that same “in-room” JID.

### Notices

Notices are received exactly like private messages.  It is not possible to
send a notice.

### Kicks and bans

Kicks are transparently translated from one protocol to another.  However
banning an XMPP participant has no effect.  To ban an user you need to set a
mode +b on that user nick or host (see *MODES*) and then kick it.

### Encoding

On XMPP, the encoding is always `UTF-8`, whereas on IRC the encoding of
each message can be anything.

This means that biboumi has to convert everything coming from IRC into UTF-8
without knowing the encoding of the received messages.  To do so, it checks
if each message is UTF-8 valid, if not it tries to convert from
`iso_8859-1` (because this appears to be the most common case, at least
on the channels I visit) to `UTF-8`.  If that conversion fails at some
point, a placeholder character `'�'` is inserted to indicate this
decoding error.

Messages are always sent in UTF-8 over IRC, no conversion is done in that
direction.

### IRC modes

One feature that doesn’t exist on XMPP but does on IRC is the `modes`.
Although some of these modes have a correspondance in the XMPP world (for
example the `+o` mode on a user corresponds to the `moderator` role in
XMPP), it is impossible to map all these modes to an XMPP feature.  To
circumvent this problem, biboumi provides a raw notification when modes are
changed, and lets the user change the modes directly.

To change modes, simply send a message starting with “`/mode`” followed by
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

`+q`

  Sets the participant’s role to `moderator` and its affiliation to `owner`.

`+a`

  Sets the participant’s role to `moderator` and its affiliation to `owner`.

`+o`

  Sets the participant’s role to `moderator` and its affiliation to  `admin`.

`+h`

  Sets the participant’s role to `moderator` and its affiliation to  `member`.

`+v`

  Sets the participant’s role to `participant` and its affiliation to `member`.

Similarly, when a biboumi user changes some participant's affiliation or role, biboumi translates that in an IRC mode change.

Affiliation set to `none`

  Sets mode to -vhoaq

Affiliation set to `member`

  Sets mode to +v-hoaq

Role set to `moderator`

  Sets mode to +h-oaq

Affiliation set to `admin`

  Sets mode to +o-aq

Affiliation set to `owner`

  Sets mode to +a-q

### Ad-hoc commands

Biboumi supports a few ad-hoc commands, as described in the XEP 0050.

  - ping: Just respond “pong”

  - hello: Provide a form, where the user enters their name, and biboumi
    responds with a nice greeting.

  - disconnect-user: Only available to the administrator. The user provides
    a list of JIDs, and a quit message. All the selected users are
    disconnected from all the IRC servers to which they were connected,
    using the provided quit message. Sending SIGINT to biboumi is equivalent
    to using this command by selecting all the connected JIDs and using the
    “Gateway shutdown” quit message, except that biboumi does not exit when
    using this ad-hoc command.

SECURITY
--------

The connection to the XMPP server can only be made on localhost.  The
XMPP server is not supposed to accept non-local connections from components.
Thus, encryption is not used to connect to the local XMPP server because it
is useless.

If compiled with the Botan library, biboumi can use TLS when communicating
with the IRC serveres.  It will first try ports 6697 and 6670 and use TLS if
it succeeds, if connection fails on both these ports, the connection is
established on port 6667 without any encryption.

Biboumi does not check if the received JIDs are properly formatted using
nodeprep.  This must be done by the XMPP server to which biboumi is directly
connected.

Note if you use a biboumi that you have no control on: remember that the
administrator of the gateway you use is able to view all your IRC
conversations, whether you’re using encryption or not.  This is exactly as
if you were running your IRC client on someone else’s server.  Only use
biboumi if you trust its administrator (or, better, if you are the
administrator) or if you don’t intend to have any private conversation.

Biboumi does not provide a way to ban users from connecting to it, has no
protection against flood or any sort of abuse that your users may cause on
the IRC servers. Some XMPP server however offer the possibility to restrict
what JID can access a gateway. Use that feature if you wish to grant access
to your biboumi instance only to a list of trusted users.

AUTHORS
-------

This software and man page are both written by Florent Le Coz.

LICENSE
-------

Biboumi is released under the zlib license.
