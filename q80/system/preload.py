"""Install and restore the persistent preload hook."""

import logging
from pathlib import Path
import tempfile

from q80 import build
from q80.config import ROOT
from q80.log import success
from q80.system.root import Root
from q80.transport.session import Session


log = logging.getLogger(__name__)


def install(session: Session) -> str:
    log.info("Install boot hook")
    hook = build.build_support(session.config, "preload_hook")
    preload_dir = ROOT / "device" / "preload"
    runtime = (preload_dir / "runtime.sh").read_text(encoding="utf-8")
    runtime = runtime.replace("@TELNET_PORT@", str(session.config.telnet_port))
    with tempfile.TemporaryDirectory(prefix="q80-boot-") as directory:
        folder = Path(directory)
        rendered = folder / "boot_runtime.sh"
        rendered.write_text(runtime, encoding="utf-8", newline="\n")
        files = [
            (hook, "/tmp/q80_boot_hook.so"),
            (rendered, "/tmp/q80_boot_runtime.sh"),
        ]
        result = Root(session).run((preload_dir / "install.sh").read_bytes(),
                                   files=tuple(files), timeout=90)
    if "PRELOAD_INSTALLED" not in result:
        raise RuntimeError("Preload install script did not confirm success")
    success(log, "Boot hook installed; reboot, then run preload status")
    return result


def restore(session: Session) -> str:
    log.info("Restore prior boot settings")
    script = ROOT / "device" / "preload" / "restore.sh"
    result = Root(session).run(script.read_bytes(), timeout=90)
    success(log, "Prior boot settings restored")
    return result
