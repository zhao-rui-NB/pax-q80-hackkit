"""Native cross-compiler invocation; compiler is configured, never assumed."""

import logging
from pathlib import Path
import subprocess

from q80.app_manifest import App
from q80.config import Config
from q80.log import success


log = logging.getLogger(__name__)


COMMON_FLAGS = [
    "-nostdlib", "-ffreestanding",
    "-fno-stack-protector", "-fno-asynchronous-unwind-tables",
    "-march=armv6", "-marm", "-mfloat-abi=soft", "-O2",
    "-Wl,--build-id=none",
]


def _check_arm(path: Path) -> None:
    data = path.read_bytes()[:20]
    if len(data) < 20 or data[:6] != b"\x7fELF\x01\x01" or int.from_bytes(data[18:20], "little") != 40:
        raise RuntimeError(f"Compiler did not produce ARM32 little-endian ELF: {path}")


def build(app: App, config: Config) -> Path:
    log.info("Build app: %s", app.app_id)
    sources = sorted(app.folder.glob("*.c"))
    if not sources:
        raise FileNotFoundError(f"No C sources in {app.folder}")
    app.output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([
        config.compiler, "-shared", "-fPIC", *COMMON_FLAGS,
        "-Wl,--hash-style=sysv", "-o", str(app.output),
        *map(str, sources),
    ], check=True)
    _check_arm(app.output)
    success(log, "App built: %s", app.output)
    return app.output


def build_support(config: Config, component: str) -> Path:
    """Build one of the three device-side access components."""
    from q80.config import ROOT

    targets = {
        "root_cmd": (ROOT / "device/aip_root/root_cmd.c", "libosal_root_cmd.so", True, "libosal.so"),
        "ped_patch": (ROOT / "device/ped/patch_tamper.c", "patch_tamper", False, None),
        "preload_hook": (ROOT / "device/preload/preload_hook.c", "libq80_boot_hook.so", True, "libq80_boot_hook.so"),
    }
    try:
        source, filename, shared, soname = targets[component]
    except KeyError as exc:
        raise ValueError(f"Unknown device component: {component}") from exc
    output = ROOT / "build" / "support" / filename
    log.info("Build device component: %s", component)
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [config.compiler]
    if shared:
        command += ["-shared", "-fPIC"]
    command += [*COMMON_FLAGS]
    if shared:
        command += ["-Wl,--hash-style=sysv", f"-Wl,-soname,{soname}"]
    command += ["-o", str(output), str(source)]
    if component in ("ped_patch", "preload_hook"):
        # Keep startup files out; resolve libc calls from the device at runtime.
        command += ["-lc"]
    subprocess.run(command, check=True)
    _check_arm(output)
    success(log, "Device component built: %s", output)
    return output
