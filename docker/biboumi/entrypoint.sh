#!/bin/bash

sed -i s/BIBOUMI_XMPP_SERVER_IP/${BIBOUMI_XMPP_SERVER_IP:-xmpp}/ /etc/biboumi/biboumi.cfg
sed -i s/BIBOUMI_HOSTNAME/${BIBOUMI_HOSTNAME:-biboumi.localhost}/ /etc/biboumi/biboumi.cfg
sed -i s/BIBOUMI_ADMIN/${BIBOUMI_ADMIN:-}/ /etc/biboumi/biboumi.cfg
sed -i s/BIBOUMI_SECRET/${BIBOUMI_SECRET:-missing_secret}/ /etc/biboumi/biboumi.cfg

echo "Running biboumi with the following conf: "
cat /etc/biboumi/biboumi.cfg

/usr/bin/biboumi /etc/biboumi/biboumi.cfg
