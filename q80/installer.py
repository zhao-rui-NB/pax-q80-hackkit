"""AIP installation uses XCB but does not build package contents."""

import logging
from pathlib import Path

from q80.aip import inspect
from q80.log import success
from q80.transport.session import Session


log = logging.getLogger(__name__)


def install(session: Session, path: Path, *, connected: bool = False) -> None:
    log.info("Install AIP: %s", path.name)
    inspect(path)
    if not connected:
        session.xcb.connect()
    session.xcb.device_command("installer", "aip", str(path.resolve()))
    success(log, "AIP installed: %s", path.name)
