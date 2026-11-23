#!/bin/sh
# Re-enter normal mode through the factory pedd -> keyman handoff.
X=/bin/busybox
[ -x "$X" ] || X=/usr/bin/busybox
S=/sys/devices/platform
P=/sys/pax/ped_trigger_state
B=/sys/devices/platform/iproc-bbl/brcm-iproc-bbl

"$X" mount -t proc proc /proc 2>/dev/null
echo "BEFORE=$("$X" cat "$P")"
echo 0 > "$B/bbl_enable_all_dis" 2>/dev/null || true
echo 0 > "$B/bbl_tmon_en_dis" || exit 1
echo 1 > "$B/bbl_unlock_dis" || exit 1
echo 0 > "$P" || exit 1

"$X" cp /tmp/q80_patch_tamper /startup/bin/q80_patch_tamper || exit 1
"$X" chmod 755 /startup/bin/q80_patch_tamper || exit 1
/startup/bin/q80_patch_tamper > /tmp/q80_patch_tamper.result || exit 1
"$X" cat /tmp/q80_patch_tamper.result
"$X" grep -q PEDD_HANDOFF_PATCH_OK /tmp/q80_patch_tamper.result || exit 1
"$X" grep -q TAMPER_CLEAR_PTRACE_OK /tmp/q80_patch_tamper.result || exit 1
"$X" rm -f /startup/bin/q80_patch_tamper

"$X" killall -TERM pedd || exit 1
waited=0
while [ "$waited" -lt 20 ]; do
    [ "$("$X" cat "$S/lcd_fb/ped")" = 0 ] &&
    [ "$("$X" cat "$S/keypad/ped")" = 0 ] &&
    [ "$("$X" cat "$S/touch_panel/ped")" = 0 ] && break
    "$X" sleep 1
    waited=$((waited + 1))
done
for node in lcd_fb keypad touch_panel; do
    value=$("$X" cat "$S/$node/ped")
    echo "PED_${node}=$value"
    [ "$value" = 0 ] || exit 2
done

"$X" cp /tmp/libosal.orig /usr/lib/libosal.so.new || exit 1
"$X" chmod 755 /usr/lib/libosal.so.new || exit 1
"$X" mv /usr/lib/libosal.so.new /usr/lib/libosal.so || exit 1
waited=0
while [ "$waited" -lt 20 ]; do
    [ -n "$("$X" pidof tm)" ] && break
    "$X" sleep 1
    waited=$((waited + 1))
done
PEDD=$("$X" pidof pedd)
KEYMAN=$("$X" pidof keyman)
TM=$("$X" pidof tm)
RUN_APP=$("$X" pidof runapp)
echo "PEDD=$PEDD"
echo "KEYMAN=$KEYMAN"
echo "TM=$TM"
echo "RUN_APP=$RUN_APP"
[ -n "$TM" ] || [ -n "$RUN_APP" ] || exit 3
