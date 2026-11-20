"""Build standalone ARM main() programs on Windows."""

import logging
from pathlib import Path
import subprocess

from q80.config import Config, ROOT
from q80.log import success


log = logging.getLogger(__name__)
SYSROOT = ROOT / "toolchain" / "sysroot"


def build(source: Path, output: Path, config: Config, libraries: tuple[str, ...] = ()) -> None:
    if not (SYSROOT / "usr/lib/crt1.o").is_file():
        raise FileNotFoundError(f"ARM sysroot missing: {SYSROOT}; run toolchain/setup.py first")
    output.parent.mkdir(parents=True, exist_ok=True)
    device_libs = ROOT / "toolchain" / "device-libs"
    for library in libraries:
        path = device_libs / f"lib{library}.so"
        if not path.is_file():
            raise FileNotFoundError(f"Device library missing: {path}")
    subprocess.run([
        config.compiler, f"--sysroot={SYSROOT}", f"-I{ROOT / 'toolchain/include'}",
        "-std=gnu99", "-march=armv6", "-marm",
        "-mfloat-abi=soft", "-Wl,--hash-style=sysv",
        # The vendor SOs depend on other device-only libraries. The Q80 loader
        # resolves those dependencies; the Windows link only needs our calls.
        "-Wl,--build-id=none", "-Wl,--allow-shlib-undefined",
        "-o", str(output), str(source),
        f"-L{device_libs}", *(f"-l{library}" for library in libraries),
    ], check=True)
    success(log, "ARM main program built: %s", output)
