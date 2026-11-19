#!/bin/sh
# One root shell for this boot. XCB writes a task, then a trigger file.
X=/bin/busybox
[ -x "$X" ] || X=/usr/bin/busybox
echo $$ > /tmp/q80_root_ready

while [ "$("$X" cat /tmp/q80_root_ready 2>/dev/null)" = "$$" ]; do
    if [ -f /tmp/q80_root_go ]; then
        "$X" rm -f /tmp/q80_root_go
        token=$("$X" cat /tmp/q80_root_request)
        case "$token" in
            ''|*[!0-9a-f]*) continue ;;
        esac
        task="/tmp/q80_task_$token.sh"
        output="/tmp/q80_output_$token"
        status="/tmp/q80_status_$token"
        "$X" sh "$task" > "$output" 2>&1
        code=$?
        "$X" rm -f "$task" /tmp/q80_root_request
        echo "$code" > "$status"
    fi
    "$X" sleep 1
done
