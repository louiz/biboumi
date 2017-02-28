# This dockerfile uses the image built using Dockerfile.base, and installs
# biboumi in it

FROM biboumi-base

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
