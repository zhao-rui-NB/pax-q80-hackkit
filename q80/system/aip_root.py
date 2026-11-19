"""Temporary root commands triggered by AIP over XCB."""

import logging
from pathlib import Path
import hashlib
import tempfile
import time
import uuid

from q80 import aip, build, installer
from q80.config import ROOT
from q80.log import success
from q80.system.root import Root
from q80.transport.session import Session


BACKUPS = ROOT / "backups"
log = logging.getLogger(__name__)


def _is_stock_library(content: bytes) -> bool:
    return content.startswith(b"\x7fELF") and b"OsSleep" in content


def backup_current_libosal(session: Session) -> Path:
    """Keep the connected device's library before replacing it."""
    session.xcb.connect()
    with tempfile.TemporaryDirectory(prefix="q80-stock-") as directory:
        pulled = Path(directory) / "libosal.so"
        session.xcb.pull("/usr/lib/libosal.so", pulled)
        content = pulled.read_bytes()
    if not _is_stock_library(content):
        raise RuntimeError("Current libosal does not look like the stock Q80 library; refusing to back it up as stock")
    digest = hashlib.sha256(content).hexdigest()
    BACKUPS.mkdir(exist_ok=True)
    target = BACKUPS / f"libosal-{digest}.so"
    if not target.exists():
        target.write_bytes(content)
    elif hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        raise RuntimeError(f"Existing backup is damaged: {target}")
    (BACKUPS / "active.txt").write_text(target.name, encoding="ascii")
    return target


def stock_backup() -> Path:
    active = BACKUPS / "active.txt"
    if not active.is_file():
        raise FileNotFoundError("No device-specific libosal backup; run aip-root start first")
    name = active.read_text(encoding="ascii").strip()
    if not name.startswith("libosal-") or Path(name).name != name:
        raise RuntimeError("Invalid active libosal backup record")
    path = BACKUPS / name
    if not path.is_file():
        raise RuntimeError(f"Stock libosal backup missing or invalid: {path}")
    content = path.read_bytes()
    if not _is_stock_library(content) or name != f"libosal-{hashlib.sha256(content).hexdigest()}.so":
        raise RuntimeError(f"Stock libosal backup missing or invalid: {path}")
    return path


def start(session: Session) -> str:
    log.info("Start temporary root command access")

    busybox = ROOT / "vendor" / "busybox-arm"
    if not busybox.is_file():
        raise FileNotFoundError(busybox)
    loader = build.build_support(session.config, "root_cmd")
    backup = backup_current_libosal(session)
    suffix = uuid.uuid4().hex[:8]
    busybox_id, loader_id = f"q80_bb_{suffix}", f"q80_rc_{suffix}"
    with tempfile.TemporaryDirectory(prefix="q80-aip-root-") as directory:
        folder = Path(directory)
        busybox_aip = aip.pack_busybox(busybox, folder / "busybox.aip", busybox_id)
        loader_aip = aip.pack_root_loader(loader, folder / "root_cmd.aip", loader_id)
        try:
            installer.install(session, busybox_aip, connected=True)
            installer.install(session, loader_aip, connected=True)
            daemon = ROOT / "device" / "aip_root" / "daemon.sh"
            session.xcb.push(daemon, "/tmp/q80_root_daemon.sh")
            spawn = folder / "spawn"
            spawn.write_bytes(b"go\n")
            session.xcb.push(spawn, "/tmp/q80_root_spawn")
            empty = folder / "spawn.aip"
            empty.touch()
            session.xcb.device_command("installer", "aip", str(empty), check=False)
            deadline = time.monotonic() + 10
            while True:
                try:
                    session.xcb.stat("/tmp/q80_root_ready")
                    break
                except FileNotFoundError:
                    if time.monotonic() >= deadline:
                        raise RuntimeError("Root daemon did not start")
                    time.sleep(0.5)
            _restore_stock_libosal(session)
        except Exception as exc:
            try:
                _restore_stock_libosal(session)
            except Exception as restore_error:
                raise RuntimeError(
                    f"aip-root start stopped: {exc}. Stock libosal could not be restored: "
                    f"{restore_error}. Backup: {backup}. Reboot before trying again."
                ) from exc
            raise RuntimeError(
                f"aip-root start stopped: {exc}. Stock libosal is restored. Backup: {backup}."
            ) from exc

    # The root daemon stays alive for this boot; libosal is already restored.
    for app_id in (busybox_id, loader_id):
        session.xcb.device_command("installer", "uaip", app_id, check=False)
    message = f"Root XCB command channel is ready. Stock libosal restored from: {backup}."
    success(log, "%s", message)
    return message


def stop(session: Session) -> str:
    """Close the root command channel for this boot."""
    session.xcb.connect()
    try:
        session.xcb.stat("/tmp/q80_root_ready")
    except FileNotFoundError:
        message = "Root XCB command channel is already stopped."
        success(log, "%s", message)
        return message
    Root(session).run("/bin/busybox rm -f /tmp/q80_root_ready", connected=True)
    message = "Root XCB command channel stopped."
    success(log, "%s", message)
    return message


def _restore_stock_libosal(session: Session) -> str:
    original = stock_backup()
    expected = hashlib.sha256(original.read_bytes()).hexdigest()
    session.xcb.connect()
    with tempfile.TemporaryDirectory(prefix="q80-check-") as directory:
        current_file = Path(directory) / "current.so"
        session.xcb.pull("/usr/lib/libosal.so", current_file)
        current = current_file.read_bytes()
        if hashlib.sha256(current).hexdigest() == expected:
            message = "Stock libosal is already installed."
            success(log, "%s", message)
            return message
        if _is_stock_library(current):
            raise RuntimeError("Device has a different stock libosal; refusing to overwrite it with an unrelated backup")
    session.xcb.push(original, "/tmp/q80_libosal_stock.so")
    command = (
        "cp /tmp/q80_libosal_stock.so /usr/lib/libosal.so.new && "
        "chmod 755 /usr/lib/libosal.so.new && "
        "mv /usr/lib/libosal.so.new /usr/lib/libosal.so"
    )
    Root(session).run(command, connected=True)
    with tempfile.TemporaryDirectory(prefix="q80-libosal-") as directory:
        copy = Path(directory) / "libosal.so"
        session.xcb.pull("/usr/lib/libosal.so", copy)
        if hashlib.sha256(copy.read_bytes()).hexdigest() != expected:
            raise RuntimeError("Restored libosal checksum differs from the stock backup")
    message = "Stock libosal restored and verified."
    success(log, "%s", message)
    return message
