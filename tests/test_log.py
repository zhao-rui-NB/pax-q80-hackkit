import io
import logging
import unittest
from contextlib import redirect_stderr
from unittest.mock import Mock

from q80.log import configure, report_error, success
from q80.transport.xcb import _read


class LogTest(unittest.TestCase):
    def test_module_prefix_and_requested_symbols(self):
        output = io.StringIO()
        logger = logging.getLogger("q80.transport.xcb")
        with redirect_stderr(output):
            configure()
            success(logger, "Connection OK")
            logger.error("Connection failed")
        configure()
        self.assertEqual(output.getvalue().splitlines(), [
            "[xcb] ✅ Connection OK",
            "[xcb] ❌ Connection failed",
        ])

    def test_info_and_debug_have_no_symbol(self):
        output = io.StringIO()
        logger = logging.getLogger("q80.transport.xcb")
        with redirect_stderr(output):
            configure(verbose=True)
            logger.info("Reconnect test device")
            logger.debug("Run gettime")
        configure()
        self.assertEqual(output.getvalue().splitlines(), [
            "[xcb] Reconnect test device",
            "[xcb] Run gettime",
        ])

    def test_xcb_socket_error_shows_xcb_module(self):
        output = io.StringIO()
        with redirect_stderr(output):
            configure()
            try:
                _read(Mock(recv=Mock(return_value=b"")), 1)
            except ConnectionError as exc:
                report_error(exc)
        configure()
        self.assertIn("[xcb] ❌ XCB server closed the connection", output.getvalue())


if __name__ == "__main__":
    unittest.main()
