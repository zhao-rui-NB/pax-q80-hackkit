"""Capture the Q80 framebuffer as a PNG."""

import base64
import logging
from pathlib import Path

from q80.log import success
from q80.transport.session import Session


log = logging.getLogger(__name__)


def capture(session: Session, output: Path) -> None:
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError("Pillow is required: python -m pip install pillow") from exc

    size_info = session.telnet.run(
        "cat /sys/class/graphics/fb0/virtual_size /sys/class/graphics/fb0/bits_per_pixel"
    ).output.splitlines()
    width, height = map(int, size_info[0].split(","))
    if int(size_info[1]) != 32:
        raise RuntimeError("Screenshot requires a 32-bit framebuffer")

    byte_count = width * height * 4
    encoded = session.telnet.run(
        f"dd if=/dev/graphics/fb0 bs={byte_count} count=1 2>/dev/null | base64",
        timeout=60,
    ).output
    data = base64.b64decode(encoded)
    if len(data) != byte_count:
        raise RuntimeError(f"Incomplete framebuffer: {len(data)} of {byte_count} bytes")
    # The Q80 panel shows these raw bytes as blue, green, red, alpha.
    image = Image.frombytes("RGB", (width, height), data, "raw", "BGRX")
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, format="PNG")

    success(log, "Screenshot saved: %s", output)
