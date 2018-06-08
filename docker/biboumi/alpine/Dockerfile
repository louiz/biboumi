# This Dockerfile creates a docker image running biboumi
#
# It is built by compiling the sources and all its dependencies
# directly inside the image.
# This is the prefered way to build the release image, used by the
# end users, in production.

FROM docker.io/alpine:latest

RUN apk add --no-cache  libidn libpq libstdc++ libuuid postgresql-libs \
        sqlite-libs udns udns && \
    apk add --no-cache --virtual .build cmake expat-dev g++ git libidn-dev \
        make postgresql-dev python2 sqlite-dev udns-dev util-linux-dev

# Install botan
RUN git clone https://github.com/randombit/botan.git && cd botan && ./configure.py --prefix=/usr && make -j8 && make install && rm -rf /botan

# Install biboumi
RUN git clone git://git.louiz.org/biboumi && mkdir ./biboumi/build && cd ./biboumi/build &&\
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr\
            -DCMAKE_BUILD_TYPE=Release\
            -DWITH_BOTAN=1\
            -DWITH_SQLITE3=1\
            -DWITH_LIBIDN=1\
            -DWITH_POSTGRESQL=1\
   && make -j8 && make install && rm -rf /biboumi

RUN adduser biboumi -D -h /home/biboumi

RUN mkdir /var/lib/biboumi
RUN chown -R biboumi:biboumi /var/lib/biboumi

COPY ./biboumi.cfg /etc/biboumi/biboumi.cfg
RUN chown -R biboumi:biboumi /etc/biboumi

RUN apk del .build

WORKDIR /home/biboumi
USER biboumi

CMD ["/usr/bin/biboumi", "/etc/biboumi/biboumi.cfg"]

