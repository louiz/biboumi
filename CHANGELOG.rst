Version 6.0 - 2017-09-17
========================

 - The LiteSQL dependency was removed. Only libsqlite3 is now necessary
   to work with the database.
 - Some JIDs can be added into users’ rosters. The component JID tells if
   biboumi is started or not, and the IRC-server JIDs tell if the user is
   currently connected to that server.
 - The RecordHistory option can now also be configured for each IRC channel,
   individually.
 - Add a global option to make all channels persistent.
 - The persistent_by_default configuration option has been added, this
   lets the administrator decide whether or not the rooms should be
   persistent or not by default, for all users.
 - Status code='332' is sent with the unavailable presences when biboumi is
   being shutdown or the connection to the IRC server is cut unexpectedly.
 - Support for botan version 1.11.x has been dropped, only version 2.x is
   supported.
 - Invitations can now be sent to any JID, not only JIDs served by the biboumi
   instance itself.
 - The history limits sent by the client when they request to join a
   channel is now supported.

Version 5.0 - 2017-05-24
========================

 - An identd server has been added.
 - Add a **persistent** option for channels. When a channel is configured
   as persistent, when the user leaves the room, biboumi stays idle and keeps
   saving the received messages in the archive, instead of leaving the channel
   entirely.  When the user re-joins the room later, biboumi sends the message
   history to her/him.  This feature can be used to make biboumi behave like
   an IRC bouncer.
 - Use the udns library instead of c-ares, for asynchronous DNS resolution.
   It’s still fully optional.
 - Update MAM implementation to version 6.0 (namespace mam:2)
 - If the client doesn’t specify any limit in its MAM and channel list request,
   the results returned by biboumi contain at most 100 messages, instead of
   the potentially huge complete result.
 - Multiline topics are now properly handled
 - Configuration options can be overridden by values found in the process env.
 - Botan’s TLS policies can be customized by the administrator, for each
   IRC server, with simple text files.
 - The IRC channel configuration form is now also available using the MUC
   configuration, in addition to the ad-hoc command.
 - Notices starting with [#channel] are considered as welcome messages coming
   from that channel, instead of private messages.

Version 4.3 - 2017-05-02
========================

  - Fix a segmentation fault that occured when trying to connect to an IRC
    server without any port configured.

Version 4.2 - 2017-04-26
========================

 - Fix a build issue when LiteSQL is absent from the system

Version 4.1 - 2017-03-21
========================

 - Works with botan 2.x, as well as botan 1.11.x

Version 4.0 - 2016-11-09
========================

 - The separator between the IRC nickname and the IRC server is now '%'
   instead of '!'. This makes things simpler (only one separator to
   remember). The distinction between a JID referring to a channel and a JID
   refering to a nickname is based on the first character (# or & by
   default, but this can be customized by the server with the ISUPPORT
   extension).
 - Handle channel invitations in both directions.
 - Add support for `JID escaping <.http://www.xmpp.org/extensions/xep-0106.html>`.
 - Save all channel messages into the database, with an ad-hoc option to
   disable this feature.
 - When joining a room, biboumi sends an history of the most recents messages
   found in the database.
 - Channel history can be retrieved using Message Archive Management.
 - Result Set Management can be used to request only parts of the IRC channel
   list.

Version 3.0 - 2016-08-03
========================

 - Support multiple-nick sessions: a user can join an IRC channel behind
   one single nick, using multiple different clients, at the same time (as
   long as each client is using the same bare JID).
 - Database support for persistant per-user per-server configuration. Add
   `LiteSQL <https://dev.louiz.org/projects/litesql>` as an optional
   dependency.
 - Add ad-hoc commands that lets each user configure various things
 - Support an after-connect command that will be sent to the server
   just after the user gets connected to it.
 - Support the sending of a PASS command.
 - Lets the users configure their username and realname, if the
   realname_customization is set to true.
 - The remote TLS certificates are checked against the system’s trusted
   CAs, unless the user used the configuration option that ignores these
   checks.
 - Lets the user set a sha-1 hash to identify a server certificate that
   should always be trusted.
 - Add an outgoing_bind option.
 - Add an ad-hoc command to forcefully disconnect a user from one or
   more servers.
 - Let the user configure the incoming encoding of an IRC server (the
   default behaviour remains unchanged: check if it’s valid utf-8 and if
   not, decode as latin-1).
 - Support `multi-prefix <http://ircv3.net/specs/extensions/multi-prefix-3.1.html>`.
 - And of course, many bufixes.
 - Run unit tests and a test suite, build the RPM and check many things
   automatically using gitlab-ci.


Version 2.0 - 2015-05-29
========================

 - List channels on an IRC server through an XMPP disco items request
 - Let the user send any arbitrary raw IRC command by sending a
   message to the IRC server’s JID.
 - By default, look for the configuration file as per the XDG
   basedir spec.
 - Support PING requests in all directions.
 - Improve the way we forward received NOTICEs by remembering to
   which users we previously sent a private message.  This improves the
   user experience when talking to NickServ.
 - Support joining key-protected channels
 - Setting a participant's role/affiliation now results in a change of IRC
   mode, instead of being ignored.  Setting Toto's affiliation to admin is
   now equivalent to “/mode +o Toto”
 - Fix the reconnection to the XMPP server to try every 2 seconds
   instead of immediately. This avoid hogging resources for nothing
 - Asynchronously resolve domain names by optionally using the DNS
   library c-ares.
 - Add a reload add-hoc command, to reload biboumi's configuration
 - Add a fixed_irc_server option.  With this option enabled,
   biboumi can only connect to the one single IRC server configured

Version 1.1 - 2014-07-16
========================

 - Fix a segmentation fault when connecting to an IRC server using IPv6

Version 1.0 - 2014-07-12
========================

 - First stable release.
 - Mostly complete MUC to IRC, and IRC to MUC support
 - Complete handling of private messages
 - Full IRC modes support: setting any IRC mode, and receiving notifications
   for every mode change
 - Verbose connection status notifications
 - Conversion from IRC formatting to XHTML-im
 - Ad-hoc commands support
 - Basic TLS support: auto-accepts all certificates, no cipher
   configuration, no way to force usage of TLS (it is used only if
   available, clear connection is automatically used as a fallback)
 - IPv6 support
