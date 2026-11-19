import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from q80.system import preload


class PreloadTest(unittest.TestCase):
    def test_install_uses_aip_root_without_telnet(self):
        session = Mock()
        session.config.telnet_port = 2323
        with patch("q80.system.preload.build.build_support", return_value=Path("hook")), \
             patch("q80.system.preload.aip_root.run", return_value="PRELOAD_INSTALLED\n") as run:
            self.assertIn("PRELOAD_INSTALLED", preload.install(session))
        session.telnet.run.assert_not_called()
        self.assertEqual(run.call_args.kwargs["files"][0],
                         (Path("hook"), "/tmp/q80_boot_hook.so"))

    def test_restore_uses_aip_root_without_telnet(self):
        session = Mock()
        with patch("q80.system.preload.aip_root.run", return_value="PRELOAD_RESTORED\n"):
            self.assertIn("PRELOAD_RESTORED", preload.restore(session))
        session.telnet.run.assert_not_called()
