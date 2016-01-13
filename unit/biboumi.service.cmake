[Unit]
Description=Biboumi, XMPP to IRC gateway
After=network.target

[Service]
Type=${SYSTEMD_SERVICE_TYPE}
ExecStart=${CMAKE_INSTALL_PREFIX}/bin/biboumi /etc/biboumi/biboumi.cfg
ExecReload=/bin/kill -s USR1 $MAINPID
WatchdogSec=10
Restart=always
User=nobody
Group=nobody

[Install]
WantedBy=multi-user.target
