# This Dockerfile creates a docker image running biboumi

FROM docker.io/fedora:latest

RUN dnf --refresh install -y\
    gcc-c++\
    cmake\
    make\
    udns-devel\
    sqlite-devel\
    libuuid-devel\
    expat-devel\
    libidn-devel\
    systemd-devel\
    git\
    python\
    && dnf clean all

# Install botan
RUN git clone https://github.com/randombit/botan.git && cd botan && ./configure.py --prefix=/usr && make -j8 && make install && ldconfig && rm -rf /botan

# Install litesql
RUN git clone git://git.louiz.org/litesql && mkdir /litesql/build && cd /litesql/build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr && make -j8 &&  cd /litesql/build && make install && ldconfig && rm -rf /litesql

# Install biboumi
RUN git clone git://git.louiz.org/biboumi && mkdir ./biboumi/build && cd ./biboumi/build &&\
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr\
            -DCMAKE_BUILD_TYPE=Release\
            -DWITH_BOTAN=1\
            -DWITH_LITESQL=1\
            -DWITH_LIBIDN=1\
            -DWITH_SYSTEMD=1\
   && make -j8 && make install && rm -rf /biboumi

RUN useradd biboumi

RUN mkdir /var/lib/biboumi
RUN chown -R biboumi:biboumi /var/lib/biboumi

COPY ./biboumi.cfg /etc/biboumi/biboumi.cfg
RUN chown -R biboumi:biboumi /etc/biboumi

COPY ./entrypoint.sh /entrypoint.sh
RUN chmod 755 /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]

USER biboumi
