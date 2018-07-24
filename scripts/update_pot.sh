#!/bin/sh

cd "$(dirname "$0")"/..
SRC_DIR=src
PO_DIR=po
POT_FILE=$PO_DIR/biboumi.pot

mkdir -p "$PO_DIR"
find "$SRC_DIR" -name \*.cpp -o -name \*.hpp | \
    xgettext -d biboumi -s --keyword=_ --add-comments=i18n -p "$PO_DIR" \
    -o biboumi.pot -f - --package-name='Biboumi' --package-version='9.0~dev' \
    --msgid-bugs-address='https://lab.louiz.org/louiz/biboumi/issues'

sed -i "s/SOME DESCRIPTIVE TITLE\./Translation of biboumi.pot to LANGUAGE/" "$POT_FILE"
sed -i "s/YEAR THE PACKAGE'S COPYRIGHT HOLDER/2013-2018/" "$POT_FILE"
sed -i "s/CHARSET/UTF-8/" "$POT_FILE"
