Biboumi Docker Image
====================

Running
-------

This image does not embed any XMPP server. You need to have a running XMPP server first: as an other docker image, or running on the host machine.

Assuming you have a running [prosody](https://hub.docker.com/r/prosody/prosody/) container already running and [properly configured](https://prosody.im/doc/components#adding_an_external_component) you can use the following command to start your biboumi container.

```
docker run --link prosody:xmpp \
    -v $PWD/database:/var/lib/biboumi \
    -e BIBOUMI_PASSWORD=P4SSW0RD \
    -e BIBOUMI_HOSTNAME=irc.example.com \
    -e BIBOUMI_ADMIN=blabla \
    biboumi
```

If instead you already have an XMPP server running on the host machine, you can start the biboumi container like this:

```
docker run --network=host \
    -v $PWD/database:/var/lib/biboumi \
    -e BIBOUMI_PASSWORD=P4SSW0RD \
    -e BIBOUMI_HOSTNAME=irc.example.com \
    -e BIBOUMI_ADMIN=blabla \
    -e BIBOUMI_XMPP_SERVER_IP=127.0.0.1 \
    biboumi
```

Variables
---------

The configuration file inside the image contains only a few default values.  To be able to run, biboumi needs additional configuration.  Additional options can be passed using environment variables.  Any configuration option can be customized this way (see biboumi’s doc), but the main are listed here for convenience:

* BIBOUMI_HOSTNAME: Sets the value of the *hostname* option.
* BIBOUMI_PASSWORD: Sets the value of the *password* option.
* BIBOUMI_ADMIN: Sets the value of the *admin* option.
* BIBOUMI_XMPP_SERVER_IP: Sets the value of the *xmpp_server_ip* option. The default value is **xmpp**.

You can also directly provide your own configuration file by mounting it inside the container using the -v option:

```
docker run --link prosody:xmpp \
    -v $PWD/biboumi.cfg:/etc/biboumi/biboumi.cfg \
    biboumi
```

If both a custom configuration file and custom environment variables are passed to the container, the environment variables will take precedence.

Linking with the XMPP server
----------------------------

You can use the --link option to connect to any server running in a docker container, but it needs to be called *xmpp*, or the custom value set for the **BIBOUMI_XMPP_SERVER_IP** option. For example, if you are using a container named ejabberd, you would use the option *--link ejabberd:xmpp*.

If you want to connect to the XMPP server running on the host machine, use the **--network=host** option.

Volumes
-------

The database is stored in the /var/lib/biboumi/ directory. If you don’t bind a local directory to it, the database will be lost when the container is stopped. If you want to keep your database between each run, bind it with the -v option, like this: **-v /srv/biboumi/:/var/lib/biboumi**.

Note: Due to a limitation in Docker, to be able to read and write into this database, make sure this mounted directory is owned by UID and GID 1001:1001, on the host.

```
chown -R 1001:1001 database/
```
