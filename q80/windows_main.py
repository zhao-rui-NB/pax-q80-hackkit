"""Build standalone ARM main() programs on Windows."""

import logging
from pathlib import Path
import subprocess

from q80.config import Config, ROOT
from q80.log import success


log = logging.getLogger(__name__)
SYSROOT = ROOT / "toolchain" / "sysroot"


def build(source: Path, output: Path, config: Config) -> None:
    if not (SYSROOT / "usr/lib/crt1.o").is_file():
        raise FileNotFoundError(f"ARM sysroot missing: {SYSROOT}; run toolchain/setup.py first")
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        config.compiler, f"--sysroot={SYSROOT}", "-std=gnu99", "-march=armv6", "-marm",
        "-mfloat-abi=soft", "-Wl,--hash-style=sysv",
        "-Wl,--build-id=none", "-o", str(output), str(source),
    ], check=True)
    success(log, "ARM main program built: %s", output)
