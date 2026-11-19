import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from q80.system.root import Root


class RootTest(unittest.TestCase):
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
        with tempfile.TemporaryDirectory() as folder, \
             patch("q80.system.root.ROOT", Path(folder)):
            output = Root(session).run("id")
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
        with tempfile.TemporaryDirectory() as folder, \
             patch("q80.system.root.ROOT", Path(folder)):
            with self.assertRaisesRegex(RuntimeError, "command failed"):
                Root(session).run("exit 3")

    def test_inactive_daemon_prompts_start_before_upload(self):
        session = Mock()
        session.xcb.stat.side_effect = FileNotFoundError("ready")
        with tempfile.TemporaryDirectory() as folder, \
             patch("q80.system.root.ROOT", Path(folder)):
            with self.assertRaisesRegex(RuntimeError, "run aip-root start first"):
                Root(session).run("id")
        session.xcb.push.assert_not_called()
        session.xcb.device_command.assert_not_called()

if __name__ == "__main__":
    unittest.main()
