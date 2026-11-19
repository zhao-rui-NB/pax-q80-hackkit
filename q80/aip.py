"""Pure AIP ZIP creation and inspection. No device connection here."""

import logging
from pathlib import Path
import stat
import zipfile

from q80.app_manifest import App
from q80.log import success


log = logging.getLogger(__name__)


def _add(archive: zipfile.ZipFile, name: str, content: bytes, mode: int = 0o755) -> None:
    info = zipfile.ZipInfo(name)
    info.create_system = 3
    info.external_attr = (stat.S_IFREG | mode) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, content)


def _link(archive: zipfile.ZipFile, name: str, target: str) -> None:
    info = zipfile.ZipInfo(name)
    info.create_system = 3
    info.external_attr = (stat.S_IFLNK | 0o777) << 16
    archive.writestr(info, target.encode("ascii"))


def pack_app(app: App, output: Path) -> Path:
    if not app.output.is_file():
        raise FileNotFoundError(f"Build output missing: {app.output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        _add(archive, "appinfo", app.appinfo, 0o664)
        _add(archive, app.binary, app.output.read_bytes())
    success(log, "App AIP packed: %s", output)
    return output


def pack_root_loader(so: Path, output: Path, app_id: str) -> Path:
    """Verified one-stage libosal replacement layout for temporary root access."""
    info = (
        f"[app]\nid={app_id}\nname=root loader\n"
        "bin=manifest.xml/libosal.so\nfile_type=dll\nversion=1.0.0\n"
    ).encode("ascii")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        _add(archive, "appinfo", info, 0o664)
        _link(archive, "manifest.xml", "/usr/lib")
        _add(archive, "manifest.xml/libosal.so", so.read_bytes(), 0o777)
    log.debug("Root loader AIP packed: %s", output)
    return output


def pack_busybox(binary: Path, output: Path, app_id: str) -> Path:
    info = (
        f"[app]\nid={app_id}\nname=aip root busybox\n"
        "bin=probe/busybox\nfile_type=dll\nversion=1.0.0\n"
    ).encode("ascii")
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w") as archive:
        _add(archive, "appinfo", info)
        _link(archive, "probe", "/bin")
        _add(archive, "probe/busybox", binary.read_bytes(), 0o777)
        _link(archive, "probe/sh", "busybox")
    log.debug("BusyBox AIP packed: %s", output)
    return output


def inspect(path: Path) -> list[tuple[str, int]]:
    with zipfile.ZipFile(path) as archive:
        if "appinfo" not in archive.namelist():
            raise ValueError("AIP has no appinfo")
        entries = [(info.filename, info.file_size) for info in archive.infolist()]
    success(log, "AIP inspected: %s (%s entries)", path.name, len(entries))
    return entries
