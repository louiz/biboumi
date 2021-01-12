# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.io/fedora:32

ENV LC_ALL C.UTF-8

RUN dnf --refresh install -y \
git \
make \
cmake \
gcc-c++ \
uuid-devel \
udns-devel \
expat-devel \
libidn-devel \
sqlite-devel \
botan2-devel \
systemd-devel \
libuuid-devel \
libgcrypt-devel \
postgresql-devel \
lcov \
libasan \
libubsan \
valgrind \
python3-pip \
python3-lxml \
python3-devel \
python3-sphinx \
python-sphinx_rtd_theme \
wget \
fedora-packager \
rpmdevtools \
&& dnf clean all

# Install slixmpp, for e2e tests
RUN git clone https://lab.louiz.org/poezio/slixmpp && pip3 install pyasn1 && cd slixmpp && python3 setup.py build && python3 setup.py install

# Install oragono, for e2e tests
RUN wget "https://github.com/oragono/oragono/releases/download/v2.0.0/oragono-2.0.0-linux-x64.tar.gz" && tar xvf oragono-2.0.0-linux-x64.tar.gz && cp oragono-2.0.0-linux-x64/oragono /usr/local/bin
