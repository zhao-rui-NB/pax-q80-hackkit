import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from q80.system.ped import bypass


class PedTest(unittest.TestCase):
    def test_bypass_uses_aip_root_without_telnet(self):
        session = Mock()
        with patch("q80.system.ped.build.build_support", return_value=Path("patch")), \
             patch("q80.system.ped.aip_root.BACKUPS", Path("backup")), \
             patch("q80.system.ped.aip_root.run", return_value="PED_OK\n") as run:
            self.assertEqual(bypass(session), "PED_OK\n")
        session.telnet.run.assert_not_called()
        self.assertEqual(run.call_args.kwargs["files"], (
            (Path("patch"), "/tmp/q80_patch_tamper"),
            (Path("backup/libosal.so"), "/tmp/libosal.orig"),
        ))
