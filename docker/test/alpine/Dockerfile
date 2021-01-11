# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.io/alpine:latest

ENV LC_ALL C.UTF-8

# Needed to build biboumi
RUN apk add --no-cache \
git \
make \
cmake \
g++ \
libuuid \
udns-dev \
expat-dev \
libidn-dev \
sqlite-dev \
botan-dev \
util-linux-dev \
libgcrypt-dev \
postgresql-dev \
valgrind \
py3-pip \
py3-lxml \
python3-dev \
libffi-dev \
go \
wget

# Install oragono, for e2e tests
RUN wget "https://github.com/oragono/oragono/archive/v2.0.0.tar.gz" && tar xvf "v2.0.0.tar.gz" && cd "oragono-2.0.0" && make && cp ~/go/bin/oragono /usr/local/bin

# Install slixmpp, for e2e tests
RUN git clone https://github.com/saghul/aiodns.git && cd aiodns && git checkout 7ee13f9bea25784322~ && python3 setup.py build && python3 setup.py install && git clone https://lab.louiz.org/poezio/slixmpp && pip3 install pyasn1 && cd slixmpp && python3 setup.py build && python3 setup.py install
