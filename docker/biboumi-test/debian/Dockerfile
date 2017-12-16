# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.io/debian:latest

ENV LC_ALL C.UTF-8

RUN apt update

# Needed to build biboumi
RUN apt install -y g++\
    clang\
    valgrind\
    libudns-dev\
    libc-ares-dev\
    libsqlite3-dev\
    libuuid1\
    libgcrypt20-dev\
    cmake\
    make\
    libexpat1-dev\
    libidn11-dev\
    uuid-dev\
    libsystemd-dev\
    pandoc\
    libasan3\
    libubsan0\
    git\
    python3-lxml\
    lcov\
    libtool\
    python3-pip\
    python3-dev\
    automake\
    autoconf\
    flex\
    bison\
    libltdl-dev\
    openssl\
    zlib1g-dev\
    libssl-dev\
    curl\
    libpq-dev

# Install botan
RUN git clone https://github.com/randombit/botan.git && cd botan && ./configure.py --prefix=/usr && make -j8 && make install && rm -rf /botan

# Install slixmpp, for e2e tests
RUN git clone https://github.com/saghul/aiodns.git && cd aiodns && git checkout 7ee13f9bea25784322~ && python3 setup.py build && python3 setup.py install && git clone git://git.louiz.org/slixmpp && pip3 install pyasn1==0.4.2 && cd slixmpp && python3 setup.py build && python3 setup.py install

RUN useradd tester -m

# Install charybdis, for e2e tests
RUN git clone https://github.com/charybdis-ircd/charybdis.git && cd charybdis && cd /charybdis && git checkout 4f2b9a4 && sed s/113/1113/ -i /charybdis/authd/providers/ident.c && ./autogen.sh && ./configure --prefix=/home/tester/ircd --bindir=/usr/bin && make -j8 && make install && rm -rf /charybdis

RUN chown -R tester:tester /home/tester/ircd

USER tester

RUN yes "" | openssl req -nodes -x509 -newkey rsa:4096 -keyout /home/tester/ircd/etc/ssl.key -out /home/tester/ircd/etc/ssl.pem

WORKDIR /home/tester
