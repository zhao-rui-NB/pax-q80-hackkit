"""One place for the configured device connections."""

import logging
from q80.config import Config
from q80.transport.telnet import Telnet
from q80.transport.xcb import Xcb


log = logging.getLogger(__name__)


class Session:
    def __init__(self, config: Config):
        self.config = config
        self.telnet = Telnet(config)
        self.xcb = Xcb(config)
        log.debug("Telnet and XCB clients ready")
