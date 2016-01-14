[Unit]
Description=Biboumi, XMPP to IRC gateway
Documentation=man:biboumi(1) http://biboumi.louiz.org
After=network.target

[Service]
Type=${SYSTEMD_SERVICE_TYPE}
ExecStart=${CMAKE_INSTALL_PREFIX}/bin/biboumi /etc/biboumi/biboumi.cfg
ExecReload=/bin/kill -s USR1 $MAINPID
WatchdogSec=${WATCHDOG_SEC}
Restart=always
User=nobody
Group=nobody

[Install]
WantedBy=multi-user.target
