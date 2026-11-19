"""Bypass PED for the current boot through the factory service handoff."""

import logging
from q80 import build
from q80.system import aip_root
from q80.config import ROOT
from q80.log import success
from q80.transport.session import Session


log = logging.getLogger(__name__)


def bypass(session: Session) -> str:
    log.info("Handle PED for this boot")
    patcher = build.build_support(session.config, "ped_patch")
    original = aip_root.BACKUPS / "libosal.so"
    script = ROOT / "device" / "ped" / "bypass.sh"
    payloads = ((patcher, "/tmp/q80_patch_tamper"),
                (original, "/tmp/libosal.orig"))
    output = aip_root.run(session, script.read_bytes(), files=payloads, timeout=90)
    success(log, "PED script complete; run ped status after Telnet is ready")
    return output
