import unittest
import hashlib
from unittest.mock import Mock, patch
from pathlib import Path
from tempfile import TemporaryDirectory

from q80.system.aip_root import start, backup_current_libosal, restore


class AipRootTest(unittest.TestCase):
    def test_start_uses_xcb_even_when_telnet_is_available(self):
        session = Mock()
        with TemporaryDirectory() as temp:
            root = Path(temp)
            busybox = root / "vendor" / "busybox-arm"
            busybox.parent.mkdir()
            busybox.touch()
            with patch("q80.system.aip_root.ROOT", root), \
                 patch("q80.system.aip_root.build.build_support", return_value=root / "loader.so"), \
                 patch("q80.system.aip_root.backup_current_libosal", return_value=root / "stock.so"), \
                 patch("q80.system.aip_root.aip.pack_busybox", return_value=root / "busybox.aip"), \
                 patch("q80.system.aip_root.aip.pack_root_loader", return_value=root / "loader.aip"), \
                 patch("q80.system.aip_root.installer.install") as install, \
                 patch("q80.system.aip_root.Root") as root_command:
                root_command.return_value.run.return_value = "AIP_ROOT_OK\n"
                start(session)
        session.telnet.run.assert_not_called()
        self.assertEqual(install.call_count, 2)

    def test_backup_uses_connected_device_library(self):
        with TemporaryDirectory() as temp, patch("q80.system.aip_root.BACKUPS", Path(temp)):
            session = Mock()
            session.xcb.pull.side_effect = lambda remote, local: local.write_bytes(b"\x7fELFstock OsSleep")
            path = backup_current_libosal(session)
            self.assertEqual(path.read_bytes(), b"\x7fELFstock OsSleep")
            self.assertEqual((Path(temp) / "active.txt").read_text(), path.name)

    def test_restore_refuses_another_stock_library(self):
        with TemporaryDirectory() as temp, patch("q80.system.aip_root.BACKUPS", Path(temp)):
            original = b"\x7fELFstock A OsSleep"
            backup = Path(temp) / f"libosal-{hashlib.sha256(original).hexdigest()}.so"
            backup.write_bytes(original)
            (Path(temp) / "active.txt").write_text(backup.name)
            session = Mock()
            session.xcb.pull.side_effect = lambda remote, local: local.write_bytes(b"\x7fELFstock B OsSleep")
            with self.assertRaisesRegex(RuntimeError, "different stock libosal"):
                restore(session)
            session.telnet.run.assert_not_called()
            session.xcb.push.assert_not_called()

    def test_restore_accepts_already_restored_library(self):
        with TemporaryDirectory() as temp, patch("q80.system.aip_root.BACKUPS", Path(temp)):
            original = b"\x7fELFstock OsSleep"
            digest = hashlib.sha256(original).hexdigest()
            backup = Path(temp) / f"libosal-{digest}.so"
            backup.write_bytes(original)
            (Path(temp) / "active.txt").write_text(backup.name)
            session = Mock()
            session.xcb.pull.side_effect = lambda remote, local: local.write_bytes(original)
            self.assertIn("already installed", restore(session))
            session.telnet.run.assert_not_called()
            session.xcb.push.assert_not_called()
