#######################
Contributing to biboumi
#######################

Biboumi’s main workplace is at https://lab.louiz.org/louiz/biboumi

The repository is also mirrored on other websites, for example on github,
but that’s mainly for the convenience of users.

Before doing anything, you can come on the `XMPP chatroom`_ to discuss your
changes, issues or ideas.


Bug reports, feature requests
-----------------------------

To open a bug report, or a feature request, please do so on `our gitlab’s
bug tracker`_.

If the bug you’re reporting is about a bad behaviour of biboumi when some XMPP
or IRC events occur, please try to reproduce the issue with a biboumi running
in log_level=0, and include the relevant logs in your bug report.

If the issue you’re reporting may have security implications, please
select the “confidential” flag in your bug report. This includes, but is not limited to:

- disclosure of private data that was supposed to be encrypted using TLS
- denial of service (crash, infinite loop, etc) that can be caused by any
  user


Code
----

To contribute code, you can do so using git: commit your changes on any
publicly available git repository and communicate us its address.  This can
be done with a `gitlab merge request`_, or a `github pull request`_ or just
by sending a message into the `XMPP chatroom`_.

It is suggested that you use gitlab’s merge requests: this will
automatically run our continuous integration tests.

It is also recommended to add some unit or end-to-end tests for the proposed
changes.


Tests
-----

There are two test suites for biboumi:

- unit tests that can be run simply using `make check`.
  These tests use the Catch test framework, are written in pure C++
  and they should always succeed, in all possible build configuration.

- a more complex end-to-end test suite. This test suite is written in python3,
  uses a specific IRC server (`charybdis`_), and only tests the most complete
  biboumi configuration (when all dependencies are used). To run it, you need
  to install various dependencies: refer to fedora’s `Dockerfile.base`_ and
  `Dockerfile`_ to see how to install charybdis, slixmpp, botan, a ssl
  certificate, etc.

  Once all the dependencies are correctly installed, the tests are run with

.. code-block:: sh

  make e2e

To run one or more specific tests, you can do something like this:

.. code-block:: sh

  make biboumi && python3 ../tests/end_to_end  self_ping  basic_handshake_success

This will run two tests, self_ping and basic_handshake_success.

To write additional tests, you need to add a Scenario
into `the __main__.py file`_. If you have problem running this end-to-end
test suite, or if you struggle with this weird code (that would be
completely normal…), don’t hesitate to ask for help.


All these tests automatically run with various configurations, on various
platforms, using gitlab CI.


Coding style
------------
Please try to follow the existing style:

- Use only spaces, not tabs.
- Curly brackets are on their own lines.
- Use this-> everywhere it’s possible.
- Don’t start class attributes with `m_` or similar.
- Type names are in PascalCase.
- Everything else is in snake_case.


.. _our gitlab’s bug tracker: https://lab.louiz.org/louiz/biboumi/issues/new
.. _gitlab merge request: https://lab.louiz.org/louiz/biboumi/merge_requests/new
.. _github pull request: https://github.com/louiz/biboumi/pulls
.. _XMPP chatroom: xmpp:biboumi@muc.poez.io
.. _Dockerfile.base: docker/biboumi-test/fedora/Dockerfile.base
.. _Dockerfile: docker/biboumi-test/fedora/Dockerfile
.. _charybdis: https://github.com/charybdis-ircd/charybdis
.. _the __main__.py file: tests/end_to_end/__main__.py
