# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.io/fedora:latest

ENV LC_ALL C.UTF-8

RUN dnf --refresh install -y\
    gcc-c++\
    clang\
    valgrind\
    udns-devel\
    c-ares-devel\
    sqlite-devel\
    libuuid-devel\
    libgcrypt-devel\
    cmake\
    make\
    expat-devel\
    libidn-devel\
    uuid-devel\
    systemd-devel\
    pandoc\
    libasan\
    libubsan\
    git\
    fedora-packager\
    python3-lxml\
    lcov\
    rpmdevtools\
    python3-devel\
    automake\
    autoconf\
    flex\
    flex-devel\
    bison\
    libtool-ltdl-devel\
    libtool\
    openssl-devel\
    which\
    java-1.8.0-openjdk\
    postgresql-devel\
    botan2-devel\
    && dnf clean all

# Install slixmpp, for e2e tests
RUN git clone git://git.louiz.org/slixmpp && pip3 install pyasn1 && cd slixmpp && python3 setup.py build && python3 setup.py install

RUN useradd tester

# Install charybdis, for e2e tests
RUN git clone https://github.com/charybdis-ircd/charybdis.git && cd charybdis && cd /charybdis && git checkout 4f2b9a4 && sed s/113/1113/ -i /charybdis/authd/providers/ident.c && ./autogen.sh && ./configure --prefix=/home/tester/ircd --bindir=/usr/bin --with-included-boost && make -j8 && make install && rm -rf /charybdis

RUN chown -R tester:tester /home/tester/ircd

USER tester

RUN yes "" | openssl req -nodes -x509 -newkey rsa:4096 -keyout /home/tester/ircd/etc/ssl.key -out /home/tester/ircd/etc/ssl.pem

COPY coverity /home/tester/coverity

WORKDIR /home/tester
