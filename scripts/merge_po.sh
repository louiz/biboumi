#!/bin/sh

cd "$(dirname "$0")"/..
PO_DIR=po
POT_FILE=$PO_DIR/biboumi.pot

find "$PO_DIR" -name \*.po \
    -exec msgmerge --quiet --update --backup=none -s {} $POT_FILE \;
