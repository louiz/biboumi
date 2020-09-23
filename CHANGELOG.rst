Version 10.0
============

For users
---------
- Direct messages are always sent to the user's bare JID. It’s now the job
  of the XMPP server to forward these messages to all online resources.
- Private messages are now always received from the server-wide JID. You
  can still use the in-room JID (#chan%irc@biboumi/NickName) to send a
  private message but the response you will receive will come from
  nickname%irc@biboumi.

Version 9.0 - 2020-09-22
========================

For users
---------
- Messages reflections are now properly cut if the body was cut before
  being to sent to IRC
- Messages from unjoined resources are now rejected instead of being accepted.
  This helps clients understand that they are not in the room (because of
  some connection issue for example).
- All commands sent to IRC servers are now throttled to avoid being
  disconnected for excess flood. The limit value can be customized using the
  ad-hoc configuration form on a server JID.
- Support for XEP-0410 Self-Ping Optimization. This will prevent clients
  which use self-ping from dropping out of the MUC if another client with
  bad connectivity is also joined from the same account.
- SASL support has been added. A new field in the Configure ad-hoc command
  lets you set a password that will be used to authenticate to the nick
  service. This replaces the cumbersome and imperfect NickServ method.

For admins
----------
- SIGHUP is now caught and reloads the configuration like SIGUSR1 and 2.
- Add a verify_certificate policy option that lets the admin disable
  certificate validation per-domain.
- The WatchdogSec value in the biboumi.service file (for systemd) now
  defaults to the empty string, which means “disabled”.  This value can
  still be set at configure time by passing the option "-DWATCHDOG_SEC=20”
  to cmake, if you want to enable the systemd watchdog.

For developers
--------------
- The end-to-end tests have been refactored, cleaned and simplified a lot.
  A tutorial and a documentation have been written. It should now be easy
  to write a test that demonstrates a bug or a missing feature.

Version 8.5 - 2020-05-09
========================

- Fix a build failure with GCC 10

Version 8.4 - 2020-02-25
========================

- Fix a possible crash that could be caused by a very well timed identd
  query

Version 8.3 - 2018-06-01
========================

- The global ad-hoc configure command is now available on biboumi’s JID in
  fixed_irc_server mode.

Version 8.2 - 2018-05-23
========================

- The users are not able to bypass the fixed mode by just configuring a
  different Address for the IRC server anymore.

Version 8.1 - 2018-05-14
========================

- Fix a crash on a raw NAMES command

Version 8.0 - 2018-05-02
========================

- GCC 4.9 or lower are not supported anymore. The minimal version is 5.0
- Add a complete='true' in MAM’s iq result when appropriate
- The archive ordering now only relies on the value of the ID, not the
  date. This means that if you manually import archives in your database (or
  mess with it somehow), biboumi will not work properly anymore, if you
  don’t make sure the ID of everything in the muclogline table is
  consistent.
- The “virtual” channel with an empty name (for example
  %irc.freenode.net@biboumi) has been entirely removed.
- Add an “Address” field in the servers’ configure form. This lets
  the user customize the address to use when connecting to a server.
  See https://lab.louiz.org/louiz/biboumi/issues/3273 for more details.
- Messages id are properly reflected to the sender
- We now properly deal with a PostgreSQL server restart: whenever the
  connection is lost with the server, we try to reconnect and re-execute the
  query once.
- A Nick field has been added in the IRC server configuration form, to let
  the user force a nickname whenever a channel on the server is joined.
- Multiple admins can now be listed in the admin field, separated with a colon.
- Missing fields in a data-form response are now properly interpreted as
  an empty value, and not the default value. Gajim users were not able to
  empty a field of type text-multi because of this issue.
- Fix an uncaught exception with botan, when policy does not allow any
  available ciphersuite.
- When the connection gets desynchronized and tries to re-join while
  biboumi thinks it has never left, biboumi now sends the whole standard
  join sequence (history, user-list, etc).

Version 7.2 - 2018-01-24
========================

- Fix compilation with gcc 4.9. This is the last version to support this
  old version.

Version 7.1 - 2018-01-22
========================

- Fix a crash happening if a user cancels a non-existing ad-hoc session

Version 7.0 - 2018-01-17
========================

- Support PostgreSQL as a database backend. See below for migration tips.
- Add a workaround for a bug in botan < 2.4 where session resumption
  would sometime result in a TLS decode error
- Add a <x xmlns="…muc#user"/> node in our private MUC messages, to help
  clients distinguish between MUC and non-MUC messages.
- Fix the identd outgoing responses: `\\r\\n` was missing, and some clients
  would ignore our messages entirely.
- Fix the iq result sent at the end of a MAM response. Some clients (e.g.
  gajim) would throw an error as a result.
- log_level configuration option is no longer ignored if the logs are written
  into journald

Sqlite3 to PostgreSQL migration
-------------------------------

If you used biboumi with the sqlite3 database backend and you want to
start using postgresql instead, follow these simple steps:

- Make sure your Sqlite3 database has the correct format by running at
  least biboumi version 6.0 against this database. Indeed: biboumi can
  upgrade your database scheme by itself automatically when it starts, but
  the migration process can only migrate from the latest known schema,
  which is the one in version 6.x and 7.x.  If you are migrating from
  version 6.x or 7.x, you have nothing to do.
- Start biboumi (at least version 7.0) with db_name configured to use
  your postgresql database: this will create an empty database, create all
  the tables with all the rights columns, ready to be filled.
- Backup your database if you value it. The migration process will not
  write anything into it, so it your data should theorically be kept
  intact, but we never know.
- Run the dump script found in biboumi’s sources:
  `<scripts/dump_sqlite3.sh>`_. Its first and only argument must be the path
  to your sqlite3 database. For example run `./scripts/dump_sqlite3.sh
  /var/lib/biboumi/biboumi.sqlite`. This will create, in your current
  directory, some sqlite files that contain instructions to be fed into
  postgresql.
- Import all the ouput files thusly created into your PostgreSQL, with
  something like this: `psql postgresql://user@password/biboumi < *.sql`.
  This takes a few minutes if your database is huge (if it contains many
  archived messages).

Version 6.1 - 2017-10-04
========================

- Fix compilation with botan 2.3
- Fix compilation with very old distributions (such as debian wheezy or
  centos 6) that ship antique softwares (sqlite3 < 3.7.14)

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
