"""Start a temporary root daemon and run commands through XCB."""

from contextlib import contextmanager
import logging
import os
from pathlib import Path
import tempfile
import time
import uuid

from q80 import aip, build, installer
from q80.config import ROOT
from q80.log import success
from q80.transport.session import Session


BACKUPS = ROOT / "backups"
log = logging.getLogger(__name__)


@contextmanager
def _command_lock():
    path = ROOT / ".q80-root.lock"
    with path.open("a+b") as lock:
        if lock.seek(0, os.SEEK_END) == 0:
            lock.write(b"\0")
            lock.flush()
        lock.seek(0)
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(lock.fileno(), msvcrt.LK_LOCK, 1)
        else:
            import fcntl
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            lock.seek(0)
            if os.name == "nt":
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def run(session: Session, script: bytes | str, *,
        files: tuple[tuple[Path, str], ...] = (), timeout: float = 90,
        connected: bool = False) -> str:
    """Send a script to the root daemon and read its result through XCB."""
    if isinstance(script, str):
        script = script.encode("utf-8")
    token = uuid.uuid4().hex[:12]
    task = f"/tmp/q80_task_{token}.sh"
    status = f"/tmp/q80_status_{token}"
    output = f"/tmp/q80_output_{token}"
    with _command_lock(), tempfile.TemporaryDirectory(prefix="q80-root-") as temporary:
        log.info("Run root script via XCB")
        temp = Path(temporary)
        xcb = session.xcb
        if not connected:
            xcb.connect()
        try:
            xcb.stat("/tmp/q80_root_ready")
        except FileNotFoundError as exc:
            raise RuntimeError("Root daemon is not ready; run aip-root start first") from exc
        for local, remote in files:
            xcb.push(local, remote)
        local_task = temp / "task.sh"
        local_task.write_bytes(script if script.endswith(b"\n") else script + b"\n")
        xcb.push(local_task, task)
        request = temp / "request"
        request.write_bytes(token.encode("ascii") + b"\n")
        trigger = temp / "trigger"
        trigger.write_bytes(b"go\n")
        xcb.push(request, "/tmp/q80_root_request")
        xcb.push(trigger, "/tmp/q80_root_go")
        result = temp / "result"
        deadline = time.monotonic() + timeout
        while True:
            try:
                xcb.pull(status, result)
                break
            except (FileNotFoundError, RuntimeError) as exc:
                if time.monotonic() >= deadline:
                    raise RuntimeError("Root daemon did not return a result") from exc
                time.sleep(0.5)
        code = int(result.read_text(encoding="ascii").strip())
        captured = temp / "output"
        xcb.pull(output, captured)
        text = captured.read_text(encoding="utf-8", errors="replace")
        if code:
            raise RuntimeError(f"Root command exited with {code}\n{text}")
        success(log, "Root script complete")
        return text


def backup_current_libosal(session: Session) -> Path:
    """Keep the connected device's library before replacing it."""
    session.xcb.connect()
    BACKUPS.mkdir(exist_ok=True)
    target = BACKUPS / "libosal.so"
    with tempfile.TemporaryDirectory(prefix="q80-stock-") as directory:
        pulled = Path(directory) / "libosal.so"
        session.xcb.pull("/usr/lib/libosal.so", pulled)
        target.write_bytes(pulled.read_bytes())
    return target


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
    run(session, "/bin/busybox rm -f /tmp/q80_root_ready", connected=True)
    message = "Root XCB command channel stopped."
    success(log, "%s", message)
    return message


def _restore_stock_libosal(session: Session) -> str:
    original = BACKUPS / "libosal.so"
    session.xcb.connect()
    session.xcb.push(original, "/tmp/q80_libosal_stock.so")
    command = (
        "cp /tmp/q80_libosal_stock.so /usr/lib/libosal.so.new && "
        "chmod 755 /usr/lib/libosal.so.new && "
        "mv /usr/lib/libosal.so.new /usr/lib/libosal.so"
    )
    run(session, command, connected=True)
    message = "Stock libosal restored."
    success(log, "%s", message)
    return message
