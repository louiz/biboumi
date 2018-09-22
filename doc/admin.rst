###########################
Administrator documentation
###########################

Usage
=====

Biboumi acts as a server, it should be run as a daemon that lives in the
background for as long as it is needed.  Note that biboumi does not
daemonize itself, this task should be done by your init system (SysVinit,
systemd, upstart).

When started, biboumi connects, without encryption (see :ref:`security`), to the
local XMPP server on the port ``5347`` and authenticates with the provided
password.  Biboumi then serves the configured ``hostname``: this means that
all XMPP stanza with a `to` JID on that domain will be forwarded to biboumi
by the XMPP server, and biboumi will only send messages coming from that
hostname.

To cleanly shutdown the component, send a SIGINT or SIGTERM signal to it.
It will send messages to all connected IRC and XMPP servers to indicate a
reason why the users are being disconnected.  Biboumi exits when the end of
communication is acknowledged by all IRC servers.  If one or more IRC
servers do not respond, biboumi will only exit if it receives the same
signal again or if a 2 seconds delay has passed.

Configuration
=============

Configuration happens in different places, with different purposes:

- The main and global configuration that specifies vital settings for the
  daemon to run, like the hostname, password etc. This is an admin-only
  configuration, and this is described in the next section.
- A TLS configuration, also admin-only, that can be either global or
  per-domain. See `TLS configuration`_ section.
- Using the :ref:`ad-hoc-commands`, each user can configure various
  settings for themself

Daemon configuration
--------------------

The configuration file is read by biboumi as it starts. The path is
specified as the only argument to the biboumi binary.

The configuration file uses a simple format of the form ``option=value``
(note that there are no spaces before or after the equal sign).

The values from the configuration file can be overridden by environment
variables, with the name all in upper case and prefixed with `BIBOUMI_`.
For example, if the environment contains “BIBOUMI_PASSWORD=blah", this will
override the value of the “password” option in the configuration file.

Sending SIGUSR1, SIGUSR2 or SIGHUP (see kill(1)) to the process will force
it to re-read the configuration and make it close and re-open the log
files. You can use this to change any configuration option at runtime, or
do a log rotation.

Options
-------

A configuration file can look something like this:

.. code-block:: ini

  hostname=biboumi.example.com
  password=mypassword
  xmpp_server_ip=127.0.0.1
  port=5347
  admin=myself@example.com
  db_name=postgresql://biboumi:password@localhost/biboumi
  realname_customization=true
  realname_from_jid=false
  log_file=
  ca_file=
  outgoing_bind=192.168.0.12


Here is a description of all available options

hostname
~~~~~~~~

Mandatory. The hostname served by the XMPP gateway.  This domain must be
configured in the XMPP server as an external component.  See the manual
for your XMPP server for more information.  For prosody, see
http://prosody.im/doc/components#adding_an_external_component

password
~~~~~~~~

Mandatory. The password used to authenticate the XMPP component to your
XMPP server.  This password must be configured in the XMPP server,
associated with the external component on *hostname*.

xmpp_server_ip
~~~~~~~~~~~~~~

The IP address to connect to the XMPP server on. The connection to the
XMPP server is unencrypted, so the biboumi instance and the server should
normally be on the same host. The default value is 127.0.0.1.

port
~~~~

The TCP port to use to connect to the local XMPP component. The default
value is 5347.

db_name
~~~~~~~

The name of the database to use. This option can only be used if biboumi
has been compiled with a database support (Sqlite3 and/or PostgreSQL). If
the value begins with the postgresql scheme, “postgresql://” or
“postgres://”, then biboumi will try to connect to the PostgreSQL database
specified by the URI. See `the PostgreSQL doc
<https://www.postgresql.org/docs/current/static/libpq-connect.html#idm46428693970032>`_
for all possible values. For example the value could be
“postgresql://user:secret@localhost”. If the value does not start with the
postgresql scheme, then it specifies a filename that will be opened with
Sqlite3. For example the value could be “/var/lib/biboumi/biboumi.sqlite”.

admin
~~~~~

The bare JID of the gateway administrator. This JID will have more
privileges than other standard users, for example some administration
ad-hoc commands will only be available to that JID.

If you need more than one administrator, separate them with a colon (:).

fixed_irc_server
~~~~~~~~~~~~~~~~

If this option contains the hostname of an IRC server (for example
irc.example.org), then biboumi will enforce the connexion to that IRC
server only.  This means that a JID like ``#chan@biboumi.example.com``
must be used instead of ``#chan%irc.example.org@biboumi.example.com``. The
`%` character loses any meaning in the JIDs.  It can appear in the JID but
will not be interpreted as a separator (thus the JID
``#channel%hello@biboumi.example.com`` points to the channel named
``#channel%hello`` on the configured IRC server) This option can for
example be used by an administrator that just wants to let their users
join their own IRC server using an XMPP client, while forbidding access to
any other IRC server.

persistent_by_default
~~~~~~~~~~~~~~~~~~~~~

If this option is set to `true`, all rooms will be persistent by default:
the value of the “persistent” option in the global configuration of each
user will be “true”, but the value of each individual room will still
default to false. This means that a user just needs to change the global
“persistent” configuration option to false in order to override this.

If it is set to false (the default value), all rooms are not persistent by
default.

Each room can be configured individually by each user, to override this
default value. See :ref:`ad-hoc-commands`.

realname_customization
~~~~~~~~~~~~~~~~~~~~~~

If this option is set to “false” (default is “true”), the users will not be
able to use the ad-hoc commands that lets them configure their realname and
username.

realname_from_jid
~~~~~~~~~~~~~~~~~

If this option is set to “true”, the realname and username of each biboumi
user will be extracted from their JID.  The realname is their bare JID, and
the username is the node-part of their JID.  Note that if
``realname_customization`` is “true”, each user will still be able to
customize their realname and username, this option just decides the default
realname and username.

If this option is set to “false” (the default value), the realname and
username of each user will be set to the nick they used to connect to the
IRC server.

webirc_password
~~~~~~~~~~~~~~~

Configure a password to be communicated to the IRC server, as part of the
WEBIRC message (see https://kiwiirc.com/docs/webirc).  If this option is
set, an additional DNS resolution of the hostname of each XMPP server will
be made when connecting to an IRC server.

log_file
~~~~~~~~

A filename into which logs are written.  If none is provided, the logs are
written on standard output.

log_level
~~~~~~~~~

Indicate what type of log messages to write in the logs.  Value can be
from 0 to 3.  0 is debug, 1 is info, 2 is warning, 3 is error.  The
default is 0, but a more practical value for production use is 1.

ca_file
~~~~~~~

Specifies which file should be used as the list of trusted CA when
negociating a TLS session. By default this value is unset and biboumi
tries a list of well-known paths.

outgoing_bind
~~~~~~~~~~~~~

An address (IPv4 or IPv6) to bind the outgoing sockets to.  If no value is
specified, it will use the one assigned by the operating system.  You can
for example use outgoing_bind=192.168.1.11 to force biboumi to use the
interface with this address.  Note that this is only used for connections
to IRC servers.

identd_port
~~~~~~~~~~~

The TCP port on which to listen for identd queries.  The default is the
standard value: 113. To be able to listen on this privileged port, biboumi
needs to have certain capabilities: on linux, using systemd, this can be
achieved by adding `AmbientCapabilities=CAP_NET_BIND_SERVICE` to the unit
file. On other systems, other solutions exist, like the portacl module on
FreeBSD.

If biboumi’s identd server is properly started, it will receive queries from
the IRC servers asking for the “identity” of each IRC connection made to it.
Biboumi will answer with a hash of the JID that made the connection. This is
useful for the IRC server to be able to distinguish the different users, and
be able to deal with the absuses without having to simply ban the IP. Without
this identd server, moderation is a lot harder, because all the different
users of a single biboumi instance all share the same IP, and they can’t be
distinguished by the IRC servers.

To disable the built-in identd, you may set identd_port to 0.

policy_directory
~~~~~~~~~~~~~~~~

A directory that should contain the policy files, used to customize
Botan’s behaviour when negociating the TLS connections with the IRC
servers. If not specified, the directory is the one where biboumi’s
configuration file is located: for example if biboumi reads its
configuration from /etc/biboumi/biboumi.cfg, the policy_directory value
will be /etc/biboumi.


TLS configuration
-----------------

Various settings of the TLS connections can be customized using policy
files. The files should be located in the directory specified by the
configuration option `policy_directory`_.  When attempting to connect to
an IRC server using TLS, biboumi will use Botan’s default TLS policy, and
then will try to load some policy files to override the values found in
these files.  For example, if policy_directory is /etc/biboumi, when
trying to connect to irc.example.com, biboumi will try to read
/etc/biboumi/policy.txt, use the values found to override the default
values, then it will try to read /etc/biboumi/irc.example.com.policy.txt
and re-override the policy with the values found in this file.

The policy.txt file applies to all the connections, and
irc.example.policy.txt will only apply (in addition to policy.txt) when
connecting to that specific server.

To see the list of possible options to configure, refer to `Botan’s TLS
documentation <https://botan.randombit.net/manual/tls.html#tls-policies>`_.
In addition to these Botan options, biboumi implements a few custom options
listed hereafter:
- verify_certificate: if this value is set to false, biboumi will not check
the certificate validity at all. The default value is true.

By default, biboumi provides a few policy files, to work around some
issues found with a few well-known IRC servers.

.. _security:

Security
========

The connection to the XMPP server can only be made on localhost.  The
XMPP server is not supposed to accept non-local connections from
components. Thus, encryption is not used to connect to the local
XMPP server because it is useless.

If compiled with the Botan library, biboumi can use TLS when communicating
with the IRC servers.  It will first try ports 6697 and 6670 and use TLS
if it succeeds, if connection fails on both these ports, the connection is
established on port 6667 without any encryption.

Biboumi does not check if the received JIDs are properly formatted using
nodeprep.  This must be done by the XMPP server to which biboumi is
directly connected.

Biboumi does not provide a way to ban users from connecting to it, has no
protection against flood or any sort of abuse that your users may cause on
the IRC servers. Some XMPP server however offer the possibility to restrict
what JID can access a gateway. Use that feature if you wish to grant access
to your biboumi instance only to a list of trusted users.



