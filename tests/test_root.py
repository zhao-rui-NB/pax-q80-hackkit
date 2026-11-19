import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from q80.system.root import Root


class RootTest(unittest.TestCase):
    def test_returns_command_output(self):
        session = Mock()

        def pull(remote: str, local: Path) -> None:
            if remote == "/usr/lib/libosal.so":
                local.write_bytes(b"[Q80 ROOT CMD]")
            else:
                local.write_text("0\n" if "status" in remote else "uid=0(root)\n", encoding="utf-8")

        session.xcb.pull.side_effect = pull
        with tempfile.TemporaryDirectory() as folder, \
             patch("q80.system.root.ROOT", Path(folder)):
            output = Root(session).run("id")
        self.assertEqual(output, "uid=0(root)\n")
        session.xcb.device_command.assert_called_once()
        self.assertEqual(session.xcb.device_command.call_args.args[:2], ("installer", "aip"))

    def test_failed_command_includes_output(self):
        session = Mock()

        def pull(remote: str, local: Path) -> None:
            if remote == "/usr/lib/libosal.so":
                local.write_bytes(b"[Q80 ROOT CMD]")
            else:
                local.write_text("3\n" if "status" in remote else "command failed\n", encoding="utf-8")

        session.xcb.pull.side_effect = pull
        with tempfile.TemporaryDirectory() as folder, \
             patch("q80.system.root.ROOT", Path(folder)):
            with self.assertRaisesRegex(RuntimeError, "command failed"):
                Root(session).run("exit 3")

    def test_inactive_loader_prompts_start_before_upload(self):
        session = Mock()
        session.xcb.pull.side_effect = lambda remote, local: local.write_bytes(b"stock library")
        with tempfile.TemporaryDirectory() as folder, \
             patch("q80.system.root.ROOT", Path(folder)):
            with self.assertRaisesRegex(RuntimeError, "run aip-root start first"):
                Root(session).run("id")
        session.xcb.push.assert_not_called()
        session.xcb.device_command.assert_not_called()


if __name__ == "__main__":
    unittest.main()
