#!/bin/bash

sed -i s/BIBOUMI_XMPP_SERVER_IP/${BIBOUMI_XMPP_SERVER_IP:-xmpp}/ /etc/biboumi/biboumi.cfg
sed -i s/BIBOUMI_HOSTNAME/${BIBOUMI_HOSTNAME:-biboumi.localhost}/ /etc/biboumi/biboumi.cfg
sed -i s/BIBOUMI_ADMIN/${BIBOUMI_ADMIN:-}/ /etc/biboumi/biboumi.cfg
sed -i s/BIBOUMI_PASSWORD/${BIBOUMI_PASSWORD:-missing_password}/ /etc/biboumi/biboumi.cfg

chown -R biboumi:biboumi /var/lib/biboumi

echo "Running biboumi with the following conf: "
cat /etc/biboumi/biboumi.cfg

exec runuser -u biboumi /usr/bin/biboumi /etc/biboumi/biboumi.cfg
