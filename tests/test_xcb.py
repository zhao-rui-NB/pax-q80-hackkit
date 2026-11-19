import io
import subprocess
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import Mock, call, patch

from q80.config import Config
from q80.transport.xcb import Xcb


class XcbCliTest(unittest.TestCase):
    def test_raw_device_command_does_not_connect(self):
        xcb = Xcb(Config(serial="com:COM12"))
        with patch.object(xcb, "connect") as connect, \
             patch.object(xcb, "command", return_value=subprocess.CompletedProcess([], 0)) as command:
            xcb.raw("gettime")
        connect.assert_not_called()
        command.assert_called_once_with("gettime", check=False)

    def test_raw_host_command_is_forwarded(self):
        xcb = Xcb(Config())
        with patch.object(xcb, "connect") as connect, \
             patch.object(xcb, "command", return_value=subprocess.CompletedProcess([], 0)) as command:
            xcb.raw("devices")
        connect.assert_not_called()
        command.assert_called_once_with("devices", check=False)

    def test_raw_failure_is_returned_without_python_exception(self):
        from q80.cli import execute

        session = Mock()
        session.xcb.raw.return_value = subprocess.CompletedProcess(["xcb.exe", "gettime"], 7)
        self.assertEqual(execute(["xcb", "gettime"], session), 7)

    def test_check_and_reconnect_use_configured_serial(self):
        from q80.cli import execute

        session = Mock()
        execute(["xcb", "check"], session)
        session.xcb.connect.assert_called_once_with()
        execute(["xcb", "reconnect"], session)
        session.xcb.reconnect.assert_called_once_with()
        session.xcb.raw.assert_not_called()

    def test_connect_probes_before_reconnecting(self):
        xcb = Xcb(Config(serial="com:COM12"))
        with patch.object(xcb, "_probe", return_value=True), \
             patch.object(xcb, "reconnect") as reconnect, \
             self.assertLogs("q80.transport.xcb", level=25) as output:
            xcb.connect()
        reconnect.assert_not_called()
        self.assertIn("Connection OK", output.output[0])

    def test_connect_reconnects_only_when_probe_fails(self):
        xcb = Xcb(Config(serial="com:COM12"))
        with patch.object(xcb, "_probe", return_value=False), \
             patch.object(xcb, "reconnect") as reconnect, \
             self.assertLogs("q80.transport.xcb", level="INFO") as output:
            xcb.connect()
        reconnect.assert_called_once_with("com:COM12")
        self.assertIn("gettime did not respond", output.output[0])

    def test_probe_uses_short_gettime(self):
        xcb = Xcb(Config(serial="com:COM12", timeout=20))
        with patch.object(xcb, "command", return_value=subprocess.CompletedProcess([], 0)) as command:
            self.assertTrue(xcb._probe("com:COM12"))
        command.assert_called_once_with("-s", "com:COM12", "gettime",
                                        check=False, timeout=2, quiet=True)

    def test_forced_reconnect_disconnects_and_checks_device(self):
        xcb = Xcb(Config(serial="com:COM12"))
        with patch.object(xcb, "command") as command, \
             patch.object(xcb, "_probe", return_value=True), \
             redirect_stdout(io.StringIO()):
            xcb.reconnect()
        self.assertEqual(command.call_args_list, [
            call("disconnect", "com:COM12", check=False, quiet=True, timeout=2),
            call("connect", "com:COM12", quiet=True, timeout=3),
        ])

    def test_reconnect_retries_when_connect_or_probe_is_not_ready(self):
        xcb = Xcb(Config(serial="com:COM12"))
        failures = [subprocess.CalledProcessError(1, ["xcb.exe", "connect"]),
                    subprocess.CompletedProcess([], 0), subprocess.CompletedProcess([], 0)]
        with patch.object(xcb, "command", side_effect=[subprocess.CompletedProcess([], 0), *failures]) as command, \
             patch.object(xcb, "_probe", side_effect=[False, True]) as probe, \
             patch("q80.transport.xcb.time.sleep") as sleep:
            xcb.reconnect()
        self.assertEqual(command.call_count, 4)
        self.assertEqual(probe.call_count, 2)
        self.assertEqual(sleep.call_count, 2)

    def test_one_shot_ls_uses_file_service(self):
        from q80.cli import execute

        session = Mock()
        session.xcb.ls.return_value = [("test", 0o100644, 12)]
        with redirect_stdout(io.StringIO()) as output:
            self.assertEqual(execute(["xcb", "ls", "/tmp"], session), 0)
        session.xcb.connect.assert_called_once_with()
        session.xcb.ls.assert_called_once_with("/tmp")
        self.assertIn("test", output.getvalue())

    def test_one_shot_push_uses_file_service(self):
        from q80.cli import execute

        session = Mock()
        self.assertEqual(execute(["xcb", "push", "local.bin", "/tmp/remote.bin"], session), 0)
        session.xcb.connect.assert_called_once_with()
        session.xcb.push.assert_called_once_with(Path("local.bin"), "/tmp/remote.bin")
        session.xcb.raw.assert_not_called()

    def test_command_does_not_set_unsupported_server_port_env(self):
        with tempfile.TemporaryDirectory() as folder:
            executable = Path(folder) / "xcb.exe"
            executable.touch()
            xcb = Xcb(Config(xcb_exe=executable))
            with patch("q80.transport.xcb.subprocess.run") as run:
                xcb.command("devices")
        self.assertNotIn("env", run.call_args.kwargs)

    def test_subprocess_failure_has_no_traceback(self):
        from q80 import cli

        error = subprocess.CalledProcessError(1, ["xcb.exe", "gettime"])
        output = io.StringIO()
        with patch("sys.argv", ["q80", "xcb", "gettime"]), \
             patch.object(cli, "execute", side_effect=error), \
             redirect_stderr(output):
            code = cli.main()
        self.assertEqual(code, 1)
        self.assertIn("[cli] ❌", output.getvalue())
        self.assertNotIn("Traceback", output.getvalue())
