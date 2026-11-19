#!/bin/sh

BUSYBOX=/usr/bin/busybox
TELNET_PORT=@TELNET_PORT@
LOG=/data/tmp/q80_boot_runtime.log
exec >>"$LOG" 2>&1

echo "=== q80_boot_runtime ==="

"$BUSYBOX" killall telnetd 2>/dev/null
"$BUSYBOX" ln -sf "$BUSYBOX" /usr/bin/sh
"$BUSYBOX" setsid "$BUSYBOX" telnetd -l /usr/bin/sh -p "$TELNET_PORT"
echo "telnetd requested"
