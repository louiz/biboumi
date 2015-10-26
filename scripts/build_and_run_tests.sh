#!/bin/sh

set -e -x

cmake .. $@
make -j$(nproc) biboumi test_suite
./test_suite
