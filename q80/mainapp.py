"""Build and install the Q80 MAINAPP menu."""

import logging

from q80 import app_manifest, build as app_build
from q80.config import ROOT, load
from q80.log import success
from q80.transport.session import Session


log = logging.getLogger(__name__)
BINARY = ROOT / "build/q80_launcher/libq80_launcher.so"
APPINFO = ROOT / "device/apps/mainapp/appinfo"


def build() -> None:
    app_build.build(app_manifest.load("mainapp"), load())


def deploy(session: Session) -> None:
    if not BINARY.is_file():
        raise FileNotFoundError(f"Build output missing: {BINARY}; run mainapp build")
    log.info("Deploy MAINAPP menu")
    shell = session.telnet
    shell.run("mkdir -p /data/app/MAINAPP /data/appinfo/MAINAPP")
    shell.push(BINARY, "/data/app/MAINAPP/libq80_launcher.so", 0o755)
    shell.push(APPINFO, "/data/appinfo/MAINAPP/appinfo", 0o664)
    shell.run(
        "chown 999:999 /data/app/MAINAPP /data/app/MAINAPP/libq80_launcher.so "
        "/data/appinfo/MAINAPP /data/appinfo/MAINAPP/appinfo"
    )
    success(log, "MAINAPP menu deployed; restart the current menu to load it")
