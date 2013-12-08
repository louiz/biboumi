BIBOUMI 1 "2013-11-21"
======================

NAME
----

Bbiboumi - XMPP gateway to IRC

SYNOPSIS
--------

`biboumi` [`config_file`]

DESCRIPTION
-----------

Biboumi is an XMPP gateway that connects to IRC servers and translates
between the two protocols. It can be used to access IRC channels using any
XMPP client as if these channels were XMPP MUCs.

OPTIONS
-------

Available options:

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

`password` (mandatory)

  The password used to authenticate the XMPP component to your XMPP server.
  This password must be configured in the XMPP server, associated with the
  external component on *hostname*.

USAGE
-----

When started, biboumi connects, without encryption (see *SECURITY*), to the
local XMPP server on the port `5347` and provides the configured password to
authenticate.  Biboumi then serves the configured `hostname`, this means
that all XMPP stanza with a `to` JID on that domain will be sent to biboumi,
and biboumi will send only send messages coming from this hostname.

When an user joins an IRC channel on an IRC server (see *Join an IRC
channel*), biboumi connects to the remote IRC server, sets the user’s nick
as requested, and then tries to join the specified channel.  If the same
user subsequently tries to connect to an other channel on the same server,
the same IRC connection is used.  If, however, an other user wants to join
an IRC channel on that same IRC server, biboumi opens a new connection to
that server.  Biboumi connects once to each IRC server, for each user on it.

### Addressing

IRC entities are represented by XMPP JIDs.  The domain part of the JID is
the domain served by biboumi, and the local part depends on the concerned
entity.

IRC channels and IRC users JIDs have a localpart formed like this:
`name`, the `'%'` separator and the `irc_server`.

For an IRC channel, the name starts with `'&'`, `'#'`, `'+'`
or `'!'`. Some other gateway implementations, as well as some IRC
clients, do not require them to be started by one of these characters,
adding an implicit `'#'` in that case.  Biboumi does not do that because
this gets confusing when trying to understand the difference between
*foo*, *#foo*, and *##foo*.

If the name starts with any other character, this represents an IRC user.

### Join an IRC channel

To join an IRC channel `#foo` on the IRC server `irc.example.com`,
join the XMPP MUC `#foo%irc.example.com@hostname`.

### Channel messages

On XMPP, unlike on IRC, the displayed order of the messages is the same for
all participants of a MUC.  Biboumi can not however provide this feature, as
it cannot know whether the IRC server has received and forwarded the
messages to other users.  This means that the order of the messages
displayed in your XMPP may not be the same than the order on other IRC
users’.

### Nicknames

On IRC, nicknames are server-wide.  This means that one user only has one
single nickname at one given time on all the channels of a server. This is
different from XMPP where an user can have a different nick on each MUC,
even if these MUCs are on the same server.

This means that the nick you choose when joining your first IRC channel on a
given IRC server will be your nickname in all other channels that you join
on that same IRC server.
If you explicitely change your nickname on one channel, your nickname will
be changed on all channels on the same server as well.

### Private messages

Private messages are handled differently on IRC and on XMPP.  On IRC, you
talk directly to one server-user: toto on the channel #foo is the same user
than toto on the channel #bar (as long as these two channels are on the same
IRC server).  Using biboumi, there is no way to receive a message from a
room participant (from a jid like #test%irc.example.com/nickname).  Instead,
private messages are received from and sent to the user (using a jid like
nickname%irc.example.com).  For conveniance and compatibility with XMPP
clients sending private messages to the MUC participants, a message sent to
#chan%irc.example.com@irc.example.net/Nickname will be redirected to
Nickname%irc.example.com@irc.example.net, although this is not the prefered
way to do it.

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
example the `+o` mode on an user corresponds to the `moderator` role
in XMPP), it is impossible to map all these modes to an XMPP feature.  To
circumvent this problem, biboumi provides a raw notification when modes are
changed, and lets the user change the modes directly.

To change modes, simply send a message starting with “`/mode`” followed
by the modes and the arguments you want to send to the IRC server.  For
example “/mode +aho louiz”.  Note that your XMPP client may
inteprete messages begining with “/” like a command.  To actually send a
message starting with a slash, you may need to start your message with
“//mode” or “/say /mode”, depending on your client.

When a mode is changed, the user is notified by a message coming from the
MUC bare JID, looking like “Mode #foo [+ov] [toto tutu]”.  In addition, if
the mode change can be translated to an XMPP feature, the user will be
notified of this XMPP event as well. For example if a mode “+o toto” is
received, then toto’s role will be changed to moderator.  The mapping
between IRC modes and XMPP features is as follow:

`+o`

  Sets the participant’s role to `moderator`.

`+a`

  Sets the participant’s role to `admin`.

`+v`

  Sets the participant’s affiliation to `member`.

SECURITY
--------

Biboumi does not provide any encryption mechanism: connection to the XMPP
server MUST be made on localhost.  The XMPP server is not supposed to accept
non-local connection from components, thus encryption is useless.  IRC
SSL/TLS is also not implemented although this could be useful for some
users, this is however not a high priority feature.

Biboumi also does not check if JIDs are properly formatted using nodeprep.
This must be done by the XMPP server to which biboumi is directly connected.

AUTHORS
-------

Written by Florent Le Coz
