# This Dockerfile creates a docker image suitable to run biboumi’s build and
# tests.  For example, it can be used on with gitlab-ci.

FROM docker.louiz.org/biboumi-test-fedora-base

# Install litesql
RUN git clone git://git.louiz.org/litesql && mkdir /litesql/build && cd /litesql/build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr && make -j8 &&  cd /litesql/build && make install && ldconfig && rm -rf /litesql

WORKDIR /home/tester
USER tester
