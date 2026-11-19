"""Small Telnet shell client and a conservative text-based file upload."""

import base64
from dataclasses import dataclass
import hashlib
import logging
from pathlib import Path
import re
import shlex
import socket
import time
import uuid

from q80.config import Config
from q80.log import success


IAC, DO, DONT, WILL, WONT, SB, SE = 255, 253, 254, 251, 252, 250, 240
log = logging.getLogger(__name__)


@dataclass(frozen=True)
class Result:
    output: str
    code: int


def quote(value: str) -> str:
    return shlex.quote(value)


class _Decoder:
    """Strip Telnet control bytes, including controls split across packets."""

    def __init__(self, sock: socket.socket):
        self.sock = sock
        self.state = "data"
        self.action = 0

    def feed(self, data: bytes) -> bytes:
        output = bytearray()
        for byte in data:
            if self.state == "data":
                if byte == IAC:
                    self.state = "iac"
                else:
                    output.append(byte)
            elif self.state == "iac":
                if byte == IAC:
                    output.append(IAC)
                    self.state = "data"
                elif byte in (DO, DONT, WILL, WONT):
                    self.action = byte
                    self.state = "option"
                elif byte == SB:
                    self.state = "sub"
                else:
                    self.state = "data"
            elif self.state == "option":
                if self.action == DO:
                    self.sock.sendall(bytes((IAC, WONT, byte)))
                elif self.action == WILL:
                    self.sock.sendall(bytes((IAC, DONT, byte)))
                self.state = "data"
            elif self.state == "sub":
                if byte == IAC:
                    self.state = "sub_iac"
            elif self.state == "sub_iac":
                self.state = "data" if byte == SE else "sub"
        return bytes(output)


class Telnet:
    def __init__(self, config: Config):
        self.config = config

    def run(self, command: str, *, check: bool = True, timeout: float | None = None) -> Result:
        if "\n" in command or "\r" in command:
            raise ValueError("Shell command must be one line")
        token = uuid.uuid4().hex
        begin = f"__Q80_{token}_BEGIN__"
        end = f"__Q80_{token}_END__"
        # The echoed input never contains a complete marker.
        line = (
            f"printf '\\n__Q80_%s_BEGIN__\\n' {quote(token)}; "
            f"sh -c {quote(command)}; q80_rc=$?; "
            f"printf '\\n__Q80_%s_END__:%s\\n' {quote(token)} \"$q80_rc\"\n"
        )
        limit = timeout or self.config.timeout
        log.debug("Run Telnet command")
        with socket.create_connection((self.config.host, self.config.telnet_port), limit) as sock:
            decoder = _Decoder(sock)
            sock.settimeout(min(limit, 0.3))
            try:
                decoder.feed(sock.recv(4096))
            except socket.timeout:
                pass
            sock.sendall(line.encode("utf-8"))
            raw = bytearray()
            deadline = time.monotonic() + limit
            end_pattern = re.compile(re.escape(end.encode()) + rb":(\d+)\r?\n")
            while not end_pattern.search(raw):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("Telnet command timed out")
                sock.settimeout(remaining)
                chunk = sock.recv(65536)
                if not chunk:
                    raise ConnectionError("Telnet connection closed before command finished")
                raw.extend(decoder.feed(chunk))
                if len(raw) > 8 * 1024 * 1024:
                    raise RuntimeError("Telnet output exceeded 8 MiB")
        output = raw.decode("utf-8", "replace").replace("\r", "")
        if begin not in output or end not in output:
            raise RuntimeError("Telnet result markers missing")
        body = output.split(begin, 1)[1].split(end, 1)[0].strip("\n")
        match = re.search(re.escape(end) + r":(\d+)", output)
        if not match:
            raise RuntimeError("Telnet exit status missing")
        result = Result(body + ("\n" if body else ""), int(match.group(1)))
        if check and result.code:
            raise RuntimeError(f"Remote exit {result.code}\n{result.output}")
        log.debug("Telnet command exit=%s", result.code)
        return result

    def push(self, local: Path, remote: str, mode: int = 0o750) -> None:
        """Upload through the shell using base64, then verify SHA-256."""
        log.info("Upload file: %s", remote)
        target = quote(remote)
        staged = quote(f"{remote}.q80-{uuid.uuid4().hex[:8]}")
        encoded = staged + ".b64"
        self.run("command -v base64 >/dev/null && command -v sha256sum >/dev/null")
        self.run(f": > {encoded}")
        try:
            with local.open("rb") as source:
                # BusyBox's interactive TTY may drop input lines near 1 KiB.
                while chunk := source.read(240):
                    value = base64.b64encode(chunk).decode("ascii")
                    self.run(f"printf '%s' '{value}' >> {encoded}")
            expected = hashlib.sha256(local.read_bytes()).hexdigest()
            self.run(f"base64 -d < {encoded} > {staged}")
            result = self.run(f"sha256sum {staged}").output.split()
            if not result or result[0].lower() != expected:
                raise RuntimeError(f"Upload checksum mismatch: {remote}")
            self.run(f"chmod {mode:o} {staged} && mv {staged} {target}")
            success(log, "Upload verified by SHA-256: %s", remote)
        finally:
            self.run(f"rm -f {encoded} {staged}", check=False)
