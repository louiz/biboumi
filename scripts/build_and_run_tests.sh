#!/bin/sh

set -e -x

cmake .. -DCMAKE_BUILD_TYPE=Debug $@
make -j$(nproc) biboumi test_suite
make -j$(nproc) check
make -j$(nproc) coverage
