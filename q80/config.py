"""Host configuration; connection details have one owner."""

from dataclasses import dataclass
import logging
from pathlib import Path
import os
import tomllib


ROOT = Path(__file__).resolve().parent.parent
log = logging.getLogger(__name__)
LOCAL_COMPILER = (ROOT / "toolchain" / "compiler" /
                  "gcc-linaro-7.5.0-2019.12-i686-mingw32_arm-linux-gnueabi" /
                  "bin" / "arm-linux-gnueabi-gcc.exe")


@dataclass(frozen=True)
class Config:
    host: str = "192.168.1.195"
    telnet_port: int = 2323
    serial: str = "com:COM12"
    xcb_exe: Path = ROOT / "vendor" / "xcb.exe"
    timeout: float = 20.0
    compiler: str = "arm-linux-gnueabi-gcc"


def load(path: Path | None = None) -> Config:
    path = path or Path(os.environ.get("Q80_CONFIG", ROOT / "q80.toml"))
    log.debug("Load config: %s", path)
    data = tomllib.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    device = data.get("device", {})
    tools = data.get("tools", {})
    xcb = Path(tools.get("xcb", Config.xcb_exe))
    if not xcb.is_absolute():
        xcb = path.parent / xcb
    return Config(
        host=device.get("host", Config.host),
        telnet_port=int(device.get("telnet_port", Config.telnet_port)),
        serial=device.get("serial", Config.serial),
        xcb_exe=xcb,
        timeout=float(device.get("timeout", Config.timeout)),
        compiler=tools.get("compiler", str(LOCAL_COMPILER)
                           if LOCAL_COMPILER.is_file() else Config.compiler),
    )
