Biboumi
=======

Biboumi is an XMPP gateway that connects to IRC servers and translates
between the two protocols. It can be used to access IRC channels using any
XMPP client as if these channels were XMPP MUCs.

It is written in modern C++14 and makes great efforts to have as little
dependencies and to be as simple as possible.

The goal is to provide a way to access most of IRC features using any XMPP
client.  It doesn’t however try to provide a complete mapping of the
features of both worlds simply because this is not useful and most probably
impossible.  For example all IRC modes are not all translatable into an XMPP
features.  Some of them are (like +m (mute) or +o (operator) modes), but
some others are IRC-specific.  If IRC is the limiting factor (for example
you cannot have a non-ASCII nickname on IRC) then biboumi doesn’t try to
work around this issue: it just enforces the rules of the IRC server by
telling the user that he/she must choose an ASCII-only nickname.  An
important goal is to keep the software (and its code) light and simple.


Install
-------
Refer to the INSTALL_ file.


Usage
-----
Read `the documentation`_.


Authors
-------
Florent Le Coz (louiz’) <louiz@louiz.org>


Contact/Support
---------------
* XMPP ChatRoom: biboumi@muc.poez.io
* Report a bug:  https://lab.louiz.org/louiz/biboumi/issues/new

Also, see the `contributing`_ page.


Licence
-------
Biboumi is Free Software.
(learn more: http://www.gnu.org/philosophy/free-sw.html)

Biboumi is released under the zlib license.
Please read the COPYING file for details.

.. _INSTALL: doc/install.rst
.. _the documentation: https://doc.biboumi.louiz.org
.. _contributing: CONTRIBUTING.rst
