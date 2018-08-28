Installation
============

The very short version:

.. code-block:: sh

   cmake . && make && ./biboumi

If that didn’t work, read on.

Dependencies
------------

Here’s the list of all the build and runtime dependencies. Because we
strive to use the smallest number of dependencies possible, many of them
are optional.  That being said, you will have the best experience using
biboumi by having all dependencies.

Tools:
~~~~~~

- A C++14 compiler (clang >= 3.4 or gcc >= 5.0 for example)
- CMake
- sphinx (optional) to build the documentation

Libraries:
~~~~~~~~~~

expat_
 Used to parse XML from the XMPP server.

libiconv_
 Encoding from anything into UTF-8

libuuid_
 Generate unique IDs

sqlite3_ or libpq_ (optional, but recommented)
 Provides a way to store various options and messages archives in a
 database. Each user of the gateway can store their own values (for
 example their prefered port, or their IRC password). Without this
 dependency, many interesting features are missing.

libidn_ (optional, but recommended)
 Provides the stringprep functionality. Without it, JIDs for IRC users are
 not provided.

udns_ (optional, but recommended)
 Asynchronously resolve domain names. This offers better reactivity and
 performances when connecting to a big number of IRC servers at the same
 time.

libbotan_ 2.x (optional, but recommended)
 Provides TLS support. Without it, IRC connections are all made in
 plain-text mode.

gcrypt_ (mandatory only if botan is absent)
 Provides the SHA-1 hash function, for the case where Botan is absent. It
 does NOT provide any TLS or encryption feature.

systemd_ (optional)
 Provides the support for a systemd service of Type=notify. This is useful only
 if you are packaging biboumi in a distribution with Systemd.

Customize
---------

The basics
~~~~~~~~~~

Once you have all the dependencies you need, configure the build system
using cmake. The cleanest way is to create a build directory, and run
everything inside it:


.. code-block:: sh

  mkdir build/ && cd build/ && cmake ..

Choosing the dependencies
~~~~~~~~~~~~~~~~~~~~~~~~~

Without any option, cmake will look for all dependencies available on the
system and use everything it finds.  If a mandatory dependency is missing
it will obviously stop and yield an error, however if an optional
dependency is missing, it will just ignore it.

To specify that you want or don’t want to use, you need to
pass an option like this:

.. code-block:: sh

  cmake .. -DWITH_XXXX=1 -DWITHOUT_XXXX=1

The `WITH_` prefix indicates that cmake should stop if that dependency can
not be found, and the `WITHOUT_` prefix indicates that this dependency
should not be used even if it is present on the system.

The `XXXX` part needs to be replaced by one of the following: BOTAN,
LIBIDN, SYSTEMD, DOC, UDNS, SQLITE3, POSTGRESQL.

Other options
~~~~~~~~~~~~~

The default build type is "Debug", if you want to build a release version,
set the CMAKE_BUILD_TYPE variable to "release", by running this command
instead:

.. code-block:: sh

    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

You can also configure many parameters of the build (like customize CFLAGS,
the install path, choose the compiler, or enabling some options like the
POLLER to use), using the ncurses interface of ccmake:

.. code-block:: sh

    ccmake ..

In ccmake, first use 'c' to configure the build system, edit the values you
need and finaly use 'g' to generate the Makefiles to build the system and
quit ccmake.

You can also configure these options using a -D command line flag.

Biboumi also has a few advanced options that are useful only in very
specific cases.

- POLLER: lets you select the poller used by biboumi, at
  compile-time. Possible values are:

  - EPOLL: use the Linux-specific epoll(7). This is the default on Linux.
  - POLL: use the standard poll(2). This is the default value on all non-Linux
    platforms.

- DEBUG_SQL_QUERIES: If set to ON, additional debug logging and timing
  will be done for every SQL query that is executed. The default is OFF.
  Please set it to ON if you intend to share your debug logs on the bug
  trackers, if your issue affects the database.

Example:

.. code-block:: sh

  cmake . -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=/usr -DWITH_BOTAN=1 -DWITHOUT_SYSTEMD=1

This command will configure the project to build a release, with TLS enabled
(using Botan) but without using Systemd (even if available on the system).


Build
-----

Once you’ve configured everything using cmake, build the software:

To build the biboumi binary, run:

.. code-block:: sh

  make


Install
-------

And then, optionaly, Install the software system-wide

.. code-block:: sh

  make install

This will install the biboumi binary, but also the man-page (if configured
with it), the policy files, the systemd unit file, etc.


Run
---

Finally, run the software using the `biboumi` binary.  Read the documentation (the
man page biboumi(1)) or the usage page.

.. _expat: http://expat.sourceforge.net/
.. _libiconv: http://www.gnu.org/software/libiconv/
.. _libuuid: http://sourceforge.net/projects/libuuid/
.. _libidn: http://www.gnu.org/software/libidn/
.. _libbotan: http://botan.randombit.net/
.. _udns: http://www.corpit.ru/mjt/udns.html
.. _sqlite3: https://sqlite.org
.. _systemd: https://www.freedesktop.org/wiki/Software/systemd/
.. _biboumi.1.rst: doc/biboumi.1.rst
.. _gcrypt: https://www.gnu.org/software/libgcrypt/
.. _libpq: https://www.postgresql.org/docs/current/static/libpq.html
