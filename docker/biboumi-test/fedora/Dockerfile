# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.io/fedora:latest

RUN dnf update -y

# Needed to build biboumi
RUN dnf install -y gcc-c++
RUN dnf install -y clang
RUN dnf install -y valgrind
RUN dnf install -y c-ares-devel
RUN dnf install -y sqlite-devel
RUN dnf install -y libuuid-devel
RUN dnf install -y cmake
RUN dnf install -y make
RUN dnf install -y expat-devel
RUN dnf install -y libidn-devel
RUN dnf install -y uuid-devel
RUN dnf install -y systemd-devel
RUN dnf install -y pandoc

# Needed to run tests
RUN dnf install -y git
RUN dnf install -y fedora-packager python3-lxml
RUN dnf install -y lcov

# To be able to create the RPM
RUN dnf install -y rpmdevtools

# Install botan
RUN git clone https://github.com/randombit/botan.git
RUN cd botan && ./configure.py --prefix=/usr && make -j8 && make install
RUN rm -rf /botan

# Install litesql
RUN git clone git://git.louiz.org/litesql
RUN mkdir /litesql/build && cd /litesql/build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr && make -j8
RUN cd /litesql/build && make install
RUN rm -rf /litesql

RUN ldconfig

# Install slixmpp, for e2e tests
RUN git clone git://git.louiz.org/slixmpp
RUN pip3 install pyasn1
RUN dnf install -y python3-devel
RUN cd slixmpp && python3 setup.py build && python3 setup.py install

RUN useradd tester

# Install charybdis, for e2e tests
RUN dnf install -y automake autoconf flex flex-devel bison libtool-ltdl-devel openssl-devel
RUN dnf install -y libtool
RUN git clone https://github.com/charybdis-ircd/charybdis.git && cd charybdis
RUN cd /charybdis && git checkout 4f2b9a4 && sed s/113/1113/ -i /charybdis/authd/providers/ident.c && ./autogen.sh && ./configure --prefix=/home/tester/ircd --bindir=/usr/bin --with-included-boost && make -j8 && make install
RUN chown -R tester:tester /home/tester/ircd
RUN rm -rf /charybdis

RUN su - tester -c "echo export LANG=en_GB.utf-8 >> /home/tester/.bashrc"

COPY coverity /home/tester/coverity
COPY sonar-scanner-2.8 /home/tester/sonar-scanner

RUN dnf install -y which java-1.8.0-openjdk

WORKDIR /home/tester
USER tester

