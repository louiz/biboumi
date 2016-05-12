# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.io/debian:latest

RUN apt update

# Needed to build biboumi
RUN apt install -y g++
RUN apt install -y clang
RUN apt install -y valgrind
RUN apt install -y libc-ares-dev
RUN apt install -y libsqlite3-dev
RUN apt install -y libuuid1
RUN apt install -y cmake
RUN apt install -y make
RUN apt install -y libexpat1-dev
RUN apt install -y libidn11-dev
RUN apt install -y uuid-dev
RUN apt install -y libsystemd-dev
RUN apt install -y ruby-ronn

# Needed to run tests
RUN apt install -y git
RUN apt install -y python3-lxml
RUN apt install -y lcov

# Install botan
RUN git clone https://github.com/randombit/botan.git
RUN cd botan && git checkout 1.11.28 && ./configure.py --prefix=/usr && make -j8 && make install
RUN rm -rf /botan

# Install litesql
RUN git clone git://git.louiz.org/litesql
RUN mkdir /litesql/build && cd /litesql/build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr && make -j8
RUN cd /litesql/build && make install
RUN rm -rf /litesql

RUN ldconfig

# Install slixmpp, for e2e tests
RUN apt install -y python3-pip
RUN git clone git://git.louiz.org/slixmpp
RUN pip3 install pyasn1
RUN apt install -y python3-dev
RUN cd slixmpp && python3 setup.py build && python3 setup.py install

RUN useradd tester -m

# Install charybdis, for e2e tests
RUN apt install -y automake autoconf flex bison libltdl-dev openssl
RUN apt install -y libtool
RUN git clone https://github.com/charybdis-ircd/charybdis.git && cd charybdis
RUN cd /charybdis && ./autogen.sh && ./configure --prefix=/home/tester/ircd --bindir=/usr/bin && make -j8 && make install
RUN chown -R tester:tester /home/tester/ircd
RUN rm -rf /charybdis

RUN apt install -y locales
RUN export LANGUAGE=en_US.UTF-8
RUN export LANG=en_US.UTF-8
RUN export LC_ALL=en_US.UTF-8
RUN locale-gen
RUN dpkg-reconfigure locales

RUN dpkg-reconfigure locales && \
    locale-gen C.UTF-8 && \
    /usr/sbin/update-locale LANG=C.UTF-8

ENV LC_ALL C.UTF-8

WORKDIR /home/tester
USER tester
