"""XCB client commands and the installer file service."""

import logging
from pathlib import Path
import socket
import stat as statmod
import struct
import subprocess
import time

from q80.config import Config
from q80.log import success


XCB_SERVER_PORT = 6037
log = logging.getLogger(__name__)


def _read(sock: socket.socket, count: int) -> bytes:
    result = bytearray()
    while len(result) < count:
        chunk = sock.recv(count - len(result))
        if not chunk:
            raise ConnectionError("XCB server closed the connection")
        result.extend(chunk)
    return bytes(result)


def _service(sock: socket.socket, name: str) -> None:
    data = name.encode()
    sock.sendall(f"{len(data):04x}".encode() + data)
    reply = _read(sock, 4)
    if reply == b"FAIL":
        size = int(_read(sock, 4), 16)
        raise RuntimeError(_read(sock, size).decode(errors="replace"))
    if reply != b"OKAY":
        raise RuntimeError(f"Unexpected XCB reply: {reply!r}")


def _request(sock: socket.socket, opcode: bytes, path: str) -> None:
    data = path.encode()
    sock.sendall(opcode + struct.pack("<I", len(data)) + data)


class Xcb:
    def __init__(self, config: Config):
        self.config = config

    def command(self, *args: str, check: bool = True,
                timeout: float | None = None, quiet: bool = False) -> subprocess.CompletedProcess:
        if not self.config.xcb_exe.is_file():
            raise FileNotFoundError(f"XCB client not found: {self.config.xcb_exe}")
        log.debug("Run XCB client")
        return subprocess.run(
            [str(self.config.xcb_exe), *map(str, args)],
            timeout=timeout or self.config.timeout, check=check,
            stdout=subprocess.DEVNULL if quiet else None,
            stderr=subprocess.DEVNULL if quiet else None,
        )

    def device_command(self, *args: str, check: bool = True,
                       timeout: float | None = None) -> subprocess.CompletedProcess:
        return self.command("-s", self.config.serial, *args, check=check, timeout=timeout)

    def connect(self, serial: str | None = None) -> None:
        serial = serial or self.config.serial
        if self._probe(serial):
            success(log, "Connection OK (gettime replied)")
            return
        log.info("gettime did not respond; reconnecting")
        self.reconnect(serial)

    def _probe(self, serial: str) -> bool:
        try:
            result = self.command("-s", serial, "gettime", check=False,
                                  timeout=min(self.config.timeout, 2), quiet=True)
            return result.returncode == 0
        except FileNotFoundError:
            raise
        except (OSError, subprocess.TimeoutExpired):
            return False

    def reconnect(self, serial: str | None = None) -> None:
        serial = serial or self.config.serial
        log.info("Reconnect configured device")
        try:
            self.command("disconnect", serial, check=False, quiet=True,
                         timeout=min(self.config.timeout, 2))
            for attempt in range(3):
                try:
                    self.command("connect", serial, quiet=True,
                                 timeout=min(self.config.timeout, 3))
                    if self._probe(serial):
                        success(log, "Connection OK (reconnected)")
                        return
                    reason = "gettime did not respond"
                except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
                    reason = "timed out" if isinstance(exc, subprocess.TimeoutExpired) else f"xcb.exe exited with {exc.returncode}"
                if attempt < 2:
                    log.info("Connect retry %s/3: %s", attempt + 2, reason)
                    time.sleep(0.5)
                else:
                    raise RuntimeError(reason)
        except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
            if isinstance(exc, subprocess.TimeoutExpired):
                reason = "timed out"
            elif isinstance(exc, subprocess.CalledProcessError):
                reason = f"xcb.exe exited with {exc.returncode}"
            else:
                reason = str(exc)
            raise RuntimeError(f"Reconnect failed: {reason}; check the device and USB/COM port") from exc

    def raw(self, *args: str) -> subprocess.CompletedProcess:
        """Forward an original XCB command without changing connection state."""
        if not args:
            raise ValueError("Missing XCB command")
        result = self.command(*args, check=False)
        if result.returncode:
            log.error("XCB command failed (exit=%s)", result.returncode)
        return result

    def _file_socket(self) -> socket.socket:
        log.debug("Open local XCB file service")
        sock = socket.create_connection(("127.0.0.1", XCB_SERVER_PORT), self.config.timeout)
        sock.settimeout(self.config.timeout)
        try:
            _service(sock, f"host:transport:{self.config.serial}")
            _service(sock, "installer:")
        except Exception:
            sock.close()
            raise
        return sock

    def stat(self, path: str) -> tuple[int, int, int]:
        log.debug("Stat %s", path)
        with self._file_socket() as sock:
            _request(sock, b"STAT", path)
            if _read(sock, 4) != b"STAT":
                raise RuntimeError("Unexpected STAT reply")
            mode, size, mtime = struct.unpack("<III", _read(sock, 12))
            if mode == 0:
                raise FileNotFoundError(path)
            return mode, size, mtime

    def ls(self, path: str) -> list[tuple[str, int, int]]:
        log.debug("List %s", path)
        entries = []
        with self._file_socket() as sock:
            _request(sock, b"LIST", path)
            while True:
                tag = _read(sock, 4)
                if tag == b"DONE":
                    _read(sock, 16)
                    return entries
                if tag == b"FAIL":
                    size = struct.unpack("<I", _read(sock, 4))[0]
                    raise RuntimeError(_read(sock, size).decode(errors="replace"))
                if tag != b"DENT":
                    raise RuntimeError(f"Unexpected LIST reply: {tag!r}")
                mode, size, _, length = struct.unpack("<IIII", _read(sock, 16))
                if length > 1024 * 1024:
                    raise RuntimeError("XCB entry name too large")
                entries.append((_read(sock, length).decode(errors="replace"), mode, size))

    def pull(self, remote: str, local: Path) -> None:
        log.debug("Pull %s", remote)
        with self._file_socket() as sock:
            _request(sock, b"RECV", remote)
            with local.open("xb") as output:
                try:
                    while True:
                        tag = _read(sock, 4)
                        if tag == b"DONE":
                            _read(sock, 4)
                            return
                        if tag == b"FAIL":
                            size = struct.unpack("<I", _read(sock, 4))[0]
                            raise RuntimeError(_read(sock, size).decode(errors="replace"))
                        if tag != b"DATA":
                            raise RuntimeError(f"Unexpected RECV reply: {tag!r}")
                        size = struct.unpack("<I", _read(sock, 4))[0]
                        if size > 16 * 1024 * 1024:
                            raise RuntimeError("XCB data chunk too large")
                        output.write(_read(sock, size))
                except Exception:
                    output.close()
                    local.unlink(missing_ok=True)
                    raise

    def push(self, local: Path, remote: str, mode: int = statmod.S_IFREG | 0o666) -> None:
        log.info("Push %s", remote)
        with self._file_socket() as sock:
            _request(sock, b"STAT", remote)
            if _read(sock, 4) != b"STAT":
                raise RuntimeError("Unexpected STAT reply")
            _read(sock, 12)
            _request(sock, b"ASK ", "size:0")
            if _read(sock, 8)[:4] != b"OKAY":
                raise RuntimeError("Unexpected ASK reply")
            _request(sock, b"SEND", f"{remote},{mode}")
            with local.open("rb") as source:
                while chunk := source.read(65536):
                    sock.sendall(b"DATA" + struct.pack("<I", len(chunk)) + chunk)
            sock.sendall(b"DONE" + struct.pack("<I", int(time.time())))
            if _read(sock, 8)[:4] != b"OKAY":
                raise RuntimeError("XCB upload failed")
            sock.sendall(b"QUIT" + struct.pack("<I", 0))
        success(log, "Push OK: %s", remote)


def format_entry(name: str, mode: int, size: int) -> str:
    kind = "d" if statmod.S_ISDIR(mode) else "l" if statmod.S_ISLNK(mode) else "-"
    return f"{kind} {size:>10}  {name}"
