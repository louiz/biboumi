#!/bin/sh

set -e -x

cmake .. $@
make -j$(nproc) biboumi test_suite
make -j$(nproc) check
