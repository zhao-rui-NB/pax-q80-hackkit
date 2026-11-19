"""Read-only device checks used during normal development."""

import logging
from q80.log import success
from q80.transport.session import Session


log = logging.getLogger(__name__)


def doctor(session: Session) -> str:
    output = session.telnet.run(
        "id; uname -a; "
        "echo PROCESSES; ps | grep -E 'pedd|keyman|tm|runapp'; "
        "echo LIBOSAL; ls -l /usr/lib/libosal.so; "
        "echo APPINFO; ls /data/appinfo",
        check=False,
    ).output
    success(log, "Device check complete")
    return output


def installed_apps(session: Session) -> str:
    output = session.telnet.run("ls /data/appinfo", check=False).output
    success(log, "App list loaded")
    return output


def ped_status(session: Session) -> str:
    output = session.telnet.run(
        "echo TRIGGER; cat /sys/pax/ped_trigger_state; "
        "echo LCD; cat /sys/devices/platform/lcd_fb/ped; "
        "echo KEYPAD; cat /sys/devices/platform/keypad/ped; "
        "echo TOUCH; cat /sys/devices/platform/touch_panel/ped",
        check=False,
    ).output
    success(log, "PED status loaded")
    return output


def preload_status(session: Session) -> str:
    output = session.telnet.run(
        "echo PRELOAD; ls -l /data/etc/ld.so.preload; "
        "echo HOOK; ls -l /data/opt/q80_boot/libq80_boot_hook.so; "
        "echo RUNTIME; ls -l /data/opt/q80_boot/q80_boot_runtime.sh; "
        "echo LOG; tail -10 /data/tmp/q80_boot_hook.log",
        check=False,
    ).output
    success(log, "Preload status loaded")
    return output
