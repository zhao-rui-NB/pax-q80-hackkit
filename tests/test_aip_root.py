import unittest
from unittest.mock import Mock, patch
from pathlib import Path
from tempfile import TemporaryDirectory

from q80.system.aip_root import start, stop, run, backup_current_libosal, _restore_stock_libosal


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
                 patch("q80.system.aip_root._restore_stock_libosal") as restore_stock, \
                 patch("q80.system.aip_root.run") as run:
                start(session)
        session.telnet.run.assert_not_called()
        self.assertEqual(install.call_count, 2)
        run.assert_not_called()
        self.assertEqual(session.xcb.device_command.call_args_list[0].args[:2], ("installer", "aip"))
        restore_stock.assert_called_once_with(session)

    def test_backup_uses_connected_device_library(self):
        with TemporaryDirectory() as temp, patch("q80.system.aip_root.BACKUPS", Path(temp)):
            session = Mock()
            session.xcb.pull.side_effect = lambda remote, local: local.write_bytes(b"\x7fELFstock OsSleep")
            path = backup_current_libosal(session)
            self.assertEqual(path, Path(temp) / "libosal.so")
            self.assertEqual(path.read_bytes(), b"\x7fELFstock OsSleep")

    def test_stop_disables_the_daemon_channel(self):
        session = Mock()
        with patch("q80.system.aip_root.run") as run:
            stop(session)
        run.assert_called_once_with(
            session, "/bin/busybox rm -f /tmp/q80_root_ready", connected=True
        )

    def test_stop_is_safe_when_already_stopped(self):
        session = Mock()
        session.xcb.stat.side_effect = FileNotFoundError("ready")
        with patch("q80.system.aip_root.run") as run:
            self.assertIn("already stopped", stop(session))
        run.assert_not_called()

    def test_start_restores_stock_when_install_fails(self):
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
                 patch("q80.system.aip_root.installer.install", side_effect=RuntimeError("install failed")), \
                 patch("q80.system.aip_root._restore_stock_libosal") as restore_stock:
                with self.assertRaisesRegex(RuntimeError, "Stock libosal is restored"):
                    start(session)
        restore_stock.assert_called_once_with(session)

    def test_restore_uses_fixed_backup(self):
        with TemporaryDirectory() as temp, patch("q80.system.aip_root.BACKUPS", Path(temp)):
            backup = Path(temp) / "libosal.so"
            backup.write_bytes(b"stock")
            session = Mock()
            with patch("q80.system.aip_root.run") as run:
                self.assertIn("restored", _restore_stock_libosal(session))
            session.xcb.push.assert_called_once_with(backup, "/tmp/q80_libosal_stock.so")
            run.assert_called_once()
            session.telnet.run.assert_not_called()


class AipRootRunTest(unittest.TestCase):
    def test_returns_command_output(self):
        session = Mock()
        request_bytes = []

        def pull(remote: str, local: Path) -> None:
            local.write_text("0\n" if "status" in remote else "uid=0(root)\n", encoding="utf-8")

        def push(local: Path, remote: str) -> None:
            if remote == "/tmp/q80_root_request":
                request_bytes.append(local.read_bytes())

        session.xcb.pull.side_effect = pull
        session.xcb.push.side_effect = push
        with TemporaryDirectory() as folder, patch("q80.system.aip_root.ROOT", Path(folder)):
            output = run(session, "id")
        self.assertEqual(output, "uid=0(root)\n")
        session.xcb.device_command.assert_not_called()
        pushed = [call.args[1] for call in session.xcb.push.call_args_list]
        self.assertEqual(pushed[-2:], ["/tmp/q80_root_request", "/tmp/q80_root_go"])
        self.assertEqual(len(request_bytes), 1)
        self.assertTrue(request_bytes[0].endswith(b"\n"))
        self.assertNotIn(b"\r", request_bytes[0])

    def test_failed_command_includes_output(self):
        session = Mock()

        def pull(remote: str, local: Path) -> None:
            local.write_text("3\n" if "status" in remote else "command failed\n", encoding="utf-8")

        session.xcb.pull.side_effect = pull
        with TemporaryDirectory() as folder, patch("q80.system.aip_root.ROOT", Path(folder)):
            with self.assertRaisesRegex(RuntimeError, "command failed"):
                run(session, "exit 3")

    def test_inactive_daemon_prompts_start_before_upload(self):
        session = Mock()
        session.xcb.stat.side_effect = FileNotFoundError("ready")
        with TemporaryDirectory() as folder, patch("q80.system.aip_root.ROOT", Path(folder)):
            with self.assertRaisesRegex(RuntimeError, "run aip-root start first"):
                run(session, "id")
        session.xcb.push.assert_not_called()
        session.xcb.device_command.assert_not_called()
