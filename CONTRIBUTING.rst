Contributing to biboumi
=======================

Biboumi’s main workplace is at https://lab.louiz.org/louiz/biboumi

The repository is also mirrored on other websites, for example on github, but
that’s mainly for the convenience of users.

Before doing anything, you can come on the `XMPP chatroom`_ to discuss your
changes, issues or ideas.

Bug reports, feature requests
-----------------------------
To open a bug report, or a feature request, please do so on
`our gitlab’s bug tracker`_.

If the issue you’re reporting may have security implications, please select
the “confidential” flag in your bug report.


Code
----
To contribute code, you can do so using git: commit your changes on any
publicly available git repository and communicate us its address.  This
can be done with a `gitlab merge request`_, or a `github pull request`_
or just by sending a message into the `XMPP chatroom`_.

It is suggested that you use gitlab’s merge requests: this will automatically
run our continuous integration tests.

It is also recommended to add some unit or end-to-end tests for the prosposed
changes.


Coding style
------------
Please try to follow the existing style:

- Use only spaces, not tabs.
- Curly brackets are on their own lines.
- Use this-> everywhere it’s possible.
- Don’t start class attributes with “m_” or similar.
- Type names are in PascalCase.
- Everything else is in snake_case.


.. _our gitlab’s bug tracker: https://lab.louiz.org/louiz/biboumi/issues/new
.. _gitlab merge request: https://lab.louiz.org/louiz/biboumi/merge_requests/new
.. _github pull request: https://github.com/louiz/biboumi/pulls
.. _XMPP chatroom: xmpp:biboumi@muc.poez.io
