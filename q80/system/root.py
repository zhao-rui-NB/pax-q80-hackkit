"""Run root scripts through the XCB file service."""

from contextlib import contextmanager
import logging
import os
from pathlib import Path
import tempfile
import time
import uuid

from q80.config import ROOT
from q80.log import success
from q80.transport.session import Session


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


class Root:
    def __init__(self, session: Session):
        self.session = session

    def run(self, script: bytes | str, *, files: tuple[tuple[Path, str], ...] = (),
            timeout: float = 90, connected: bool = False) -> str:
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
            xcb = self.session.xcb
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
