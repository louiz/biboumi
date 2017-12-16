# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.io/alpine:latest

ENV LC_ALL C.UTF-8

# Needed to build biboumi
RUN apk add --no-cache g++\
    clang\
    valgrind\
    udns-dev\
    sqlite-dev\
    libuuid\
    util-linux-dev\
    libgcrypt-dev\
    cmake\
    make\
    expat-dev\
    libidn-dev\
    git\
    py3-lxml\
    libtool\
    py3-pip\
    python2\
    python3-dev\
    automake\
    autoconf\
    flex\
    bison\
    libltdl\
    openssl\
    libressl-dev\
    zlib-dev\
    curl\
    postgresql-dev

# Install botan
RUN git clone https://github.com/randombit/botan.git && cd botan && ./configure.py --prefix=/usr && make -j8 && make install && rm -rf /botan

# Install slixmpp, for e2e tests
RUN git clone https://github.com/saghul/aiodns.git && cd aiodns && git checkout 7ee13f9bea25784322~ && python3 setup.py build && python3 setup.py install && git clone git://git.louiz.org/slixmpp && pip3 install pyasn1 && cd slixmpp && python3 setup.py build && python3 setup.py install

RUN adduser tester -D -h /home/tester

# Install charybdis, for e2e tests
RUN git clone https://github.com/charybdis-ircd/charybdis.git && cd charybdis && cd /charybdis && git checkout 4f2b9a4 && sed s/113/1113/ -i /charybdis/authd/providers/ident.c && ./autogen.sh && ./configure --prefix=/home/tester/ircd --bindir=/usr/bin && make -j8 && make install && rm -rf /charybdis

RUN chown -R tester:tester /home/tester/ircd

RUN yes "" | openssl req -nodes -x509 -newkey rsa:4096 -keyout /home/tester/ircd/etc/ssl.key -out /home/tester/ircd/etc/ssl.pem

WORKDIR /home/tester
USER tester
