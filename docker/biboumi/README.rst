Biboumi Docker Image
====================

Running
-------

This image does not embed any XMPP server. You need to have a running XMPP
server (as an other docker container for example) first.

Assuming you have a running `prosody
<https://hub.docker.com/r/prosody/prosody/>`_ container already running and
`properly configured
<https://prosody.im/doc/components#adding_an_external_component>`_ you can
use the following command to start your biboumi container.

```
docker run --link prosody:xmpp \
    -e BIBOUMI_PASSWORD=P4SSW0RD \
    -e BIBOUMI_HOSTNAME=irc.example.com \
    -e BIBOUMI_ADMIN=blabla \
    biboumi
```

Variables
---------

The configuration file inside the image is a template that is completed when
the container is started, using the following environment variables:

* BIBOUMI_HOSTNAME: Sets the value of the *hostname* option.
* BIBOUMI_SECRET: Sets the value of the *password* option.
* BIBOUMI_ADMIN: Sets the value of the *admin* option.

All these variables are optional, but biboumi will probably fail to start if
the hostname and secret are missing.

You can also directly provide your own configuration file by mounting it
inside the container using the -v option:

```
docker run --link prosody:xmpp \
    -v $PWD/biboumi.cfg:/etc/biboumi/biboumi.cfg \
    biboumi
```

Linking with the XMPP server
----------------------------

You can use the --link option to connect to any server, but it needs to be
called *xmpp*. For example, if you are using a container named ejabberd, you
would use the option *--link ejabberd:xmpp*.
