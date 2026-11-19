import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from q80.system.ped import bypass


class PedTest(unittest.TestCase):
    def test_bypass_uses_aip_root_without_telnet(self):
        session = Mock()
        with patch("q80.system.ped.build.build_support", return_value=Path("patch")), \
             patch("q80.system.ped.aip_root.stock_backup", return_value=Path("original")), \
             patch("q80.system.ped.Root") as root:
            root.return_value.run.return_value = "PED_OK\n"
            self.assertEqual(bypass(session), "PED_OK\n")
        session.telnet.run.assert_not_called()
        self.assertEqual(root.return_value.run.call_args.kwargs["files"], (
            (Path("patch"), "/tmp/q80_patch_tamper"),
            (Path("original"), "/tmp/libosal.orig"),
        ))
