INSTALL
=======

tl;dr
-----

  cmake . && make && ./biboumi

If that didn’t work, read on.

Dependencies
------------

Build and runtime dependencies:

Tools:
~~~~~~

- A C++14 compiler (clang >= 3.4 or gcc >= 4.9 for example)
- CMake
- pandoc (optional) to build the man page

Libraries:
~~~~~~~~~~

expat_
 Used to parse XML from the XMPP server.

libiconv_
 Encoding from anything into UTF-8

libuuid_
 Generate unique IDs

sqlite3_ (option, but highly recommended)
 Provides a way to store various options in a (sqlite3) database. Each user
 of the gateway can store their own values (for example their prefered port,
 or their IRC password). Without this dependency, many interesting features
 are missing.

libidn_ (optional, but recommended)
 Provides the stringprep functionality. Without it, JIDs for IRC users are
 not provided.

udns_ (optional, but recommended)
 Asynchronously resolve domain names. This offers better reactivity and
 performances when connecting to a big number of IRC servers at the same
 time.

libbotan_ 2.x (optional)
 Provides TLS support. Without it, IRC connections are all made in
 plain-text mode.

gcrypt_ (mandatory only if botan is absent)
 Provides the SHA-1 hash function, for the case where Botan is absent.

systemd_ (optional)
 Provides the support for a systemd service of Type=notify. This is useful only
 if you are packaging biboumi in a distribution with Systemd.


Configure
---------

Configure the build system using cmake, there are many solutions to do that,
the simplest is to just run

  cmake .

in the current directory.

The default build type is "Debug", if you want to build a release version,
set the CMAKE_BUILD_TYPE variable to "release", by running this command
instead:

    cmake . -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=/usr

You can also configure many parameters of the build (like customize CFLAGS,
the install path, choose the compiler, or enabling some options like the
POLLER to use), using the ncurses interface of ccmake:

    ccmake .

In ccmake, first use 'c' to configure the build system, edit the values you
need and finaly use 'g' to generate the Makefiles to build the system and
quit ccmake.

You can also configure these options using a -D command line flag.

The list of available options:

- POLLER: lets you select the poller used by biboumi, at
  compile-time. Possible values are:

  - EPOLL: use the Linux-specific epoll(7). This is the default on Linux.
  - POLL: use the standard poll(2). This is the default value on all non-Linux
    platforms.

- WITH_BOTAN and WITHOUT_BOTAN: The first force the usage of the Botan library,
  if it is not found, the configuration process will fail. The second will
  make the build process ignore the Botan library, it will not be used even
  if it's available on the system.  If none of these option is specified, the
  library will be used if available and will be ignored otherwise.

- WITH_LIBIDN and WITHOUT_LIBIDN: Just like the WITH(OUT)_BOTAN options, but
  for the IDN library

- WITH_SYSTEMD and WITHOUT_SYSTEMD: Just like the other WITH(OUT)_* options,
  but for the Systemd library

Example:

  cmake . -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX=/usr
  -DWITH_BOTAN=1 -DWITHOUT_SYSTEMD=1

This command will configure the project to build a release, with TLS enabled
(using Botan) but without using Systemd (even if available on the system).


Build
-----
Once you’ve configured everything using cmake, build the software:

To build the biboumi binary:

  make


Install
-------
And then, optionaly, Install the software system-wide

  make install


Testing
-------
You can run the test suite with

  make check

This project uses the Catch unit test framework, it will be automatically
fetched with cmake, by cloning the github repository.

You can also check the overall code coverage of this test suite by running

  make coverage

This requires gcov and lcov to be installed.


Run
---
Run the software using the `biboumi` binary.  Read the documentation (the
man page biboumi(1) or the `biboumi.1.rst`_ file) for more information on how
to use biboumi.

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
