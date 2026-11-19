#!/bin/sh
set -e
trap 'rm -f /tmp/q80_boot_hook.so /tmp/q80_boot_runtime.sh' EXIT
DIR=/data/opt/q80_boot
PRELOAD=/data/etc/ld.so.preload
mkdir -p "$DIR" /data/etc

# Keep the previous working hook and preload as a restore point.
for name in libq80_boot_hook.so q80_boot_runtime.sh busybox; do
    if [ -f "$DIR/$name" ]; then cp -p "$DIR/$name" "$DIR/$name.bak"; fi
done
if [ -f "$PRELOAD" ]; then cp -p "$PRELOAD" "$PRELOAD.bak"; fi
touch "$DIR/.q80-hackkit-backup-ready"

BUSYBOX=/usr/bin/busybox
[ -x "$BUSYBOX" ] || BUSYBOX=/bin/busybox
test -x "$BUSYBOX"
cp "$BUSYBOX" "$DIR/busybox.new"
cp /tmp/q80_boot_hook.so "$DIR/libq80_boot_hook.so.new"
cp /tmp/q80_boot_runtime.sh "$DIR/q80_boot_runtime.sh.new"
chmod 755 "$DIR/busybox.new" "$DIR/libq80_boot_hook.so.new" "$DIR/q80_boot_runtime.sh.new"
mv "$DIR/busybox.new" "$DIR/busybox"
mv "$DIR/libq80_boot_hook.so.new" "$DIR/libq80_boot_hook.so"
mv "$DIR/q80_boot_runtime.sh.new" "$DIR/q80_boot_runtime.sh"
printf '%s\n' /data/opt/q80_boot/libq80_boot_hook.so > "$PRELOAD.new"
chmod 644 "$PRELOAD.new"
mv "$PRELOAD.new" "$PRELOAD"
sync
echo PRELOAD_INSTALLED
