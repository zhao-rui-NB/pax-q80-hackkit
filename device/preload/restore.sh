#!/bin/sh
set -e
DIR=/data/opt/q80_boot
PRELOAD=/data/etc/ld.so.preload
test -f "$DIR/.q80-hackkit-backup-ready" || {
    echo NO_HACKKIT_BACKUP
    exit 1
}
for name in libq80_boot_hook.so q80_boot_runtime.sh busybox; do
    if [ -f "$DIR/$name.bak" ]; then cp -p "$DIR/$name.bak" "$DIR/$name"; fi
done
if [ -f "$PRELOAD.bak" ]; then
    cp -p "$PRELOAD.bak" "$PRELOAD"
else
    rm -f "$PRELOAD"
fi
sync
rm -f "$DIR/.q80-hackkit-backup-ready"
echo PRELOAD_RESTORED
