"""Execute scripts through the temporary libosal loader when Telnet is unavailable."""

from contextlib import contextmanager
import logging
import os
from pathlib import Path
import tempfile
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
        """Run a script and read back its output and exit status through XCB."""
        if isinstance(script, str):
            script = script.encode("utf-8")
        token = uuid.uuid4().hex[:12]
        task = f"/tmp/q80_task_{token}.sh"
        status = f"/tmp/q80_status_{token}"
        output = f"/tmp/q80_output_{token}"
        with _command_lock(), tempfile.TemporaryDirectory(prefix="q80-root-") as temporary:
            log.info("Run root script via AIP")
            temp = Path(temporary)
            xcb = self.session.xcb
            if not connected:
                xcb.connect()
            library = temp / "libosal.so"
            xcb.pull("/usr/lib/libosal.so", library)
            if b"[Q80 ROOT CMD]" not in library.read_bytes():
                raise RuntimeError("AIP root channel is not active; run aip-root start first")
            for local, remote in files:
                xcb.push(local, remote)
            local_task = temp / "task.sh"
            local_task.write_bytes(script if script.endswith(b"\n") else script + b"\n")
            xcb.push(local_task, task)
            command = temp / "command"
            command.write_text(
                "B=/bin/busybox; [ -x \"$B\" ] || B=/usr/bin/busybox; "
                f"\"$B\" sh {task} > {output} 2>&1; Q80_RC=$?; "
                f"echo $Q80_RC > {status}; \"$B\" rm -f {task}\n",
                encoding="ascii",
            )
            trigger = temp / "trigger"
            trigger.write_bytes(b"go\n")
            empty = temp / "trigger.aip"
            empty.touch()
            xcb.push(command, "/tmp/q80_cmd")
            xcb.push(trigger, "/tmp/q80_go")
            # The empty AIP starts installer; the constructor handles the script.
            xcb.device_command("installer", "aip", str(empty), check=False, timeout=timeout)
            result = temp / "result"
            try:
                xcb.pull(status, result)
            except (FileNotFoundError, RuntimeError) as exc:
                raise RuntimeError(
                    f"AIP root command did not produce a result file: {exc}"
                ) from exc
            code = int(result.read_text(encoding="ascii").strip())
            captured = temp / "output"
            xcb.pull(output, captured)
            text = captured.read_text(encoding="utf-8", errors="replace")
            if code:
                raise RuntimeError(f"Root command exited with {code}\n{text}")
            success(log, "Root script complete")
            return text
