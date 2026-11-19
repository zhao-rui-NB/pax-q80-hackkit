import contextlib
import io
import unittest
from unittest.mock import Mock, patch

from q80 import cli
from q80.cli import execute


class CliNamesTest(unittest.TestCase):
    def test_aip_root_exec_displays_output(self):
        session = Mock()
        output = io.StringIO()
        with patch("q80.cli.aip_root.run", return_value="uid=0(root)\n") as run, \
             contextlib.redirect_stdout(output):
            execute(["aip-root", "exec", "id"], session)
        run.assert_called_once_with(session, "id")
        self.assertEqual(output.getvalue(), "uid=0(root)\n")

    def test_named_entry_points(self):
        session = Mock()
        cases = (
            (["aip-root", "start"], "q80.cli.aip_root.start"),
            (["aip-root", "stop"], "q80.cli.aip_root.stop"),
            (["preload", "install"], "q80.cli.preload.install"),
            (["preload", "restore"], "q80.cli.preload.restore"),
            (["preload", "status"], "q80.cli.diagnostics.preload_status"),
            (["ped", "bypass"], "q80.cli.ped.bypass"),
        )
        for words, target in cases:
            with self.subTest(words=words), patch(target) as action, \
                 contextlib.redirect_stdout(io.StringIO()):
                execute(words, session)
                action.assert_called_once_with(session)

    def test_aip_root_restore_is_not_a_command(self):
        with self.assertRaises(ValueError):
            execute(["aip-root", "restore"], Mock())

    def test_help_matches_interactive_help_without_loading_config(self):
        one_shot = io.StringIO()
        interactive = io.StringIO()
        with patch("sys.argv", ["q80", "--help"]), \
             patch("q80.cli.load") as load, \
             contextlib.redirect_stdout(one_shot):
            self.assertEqual(cli.main(), 0)
        load.assert_not_called()
        with contextlib.redirect_stdout(interactive):
            execute(["help"], Mock())
        self.assertEqual(one_shot.getvalue(), interactive.getvalue())
        self.assertIn("ped", one_shot.getvalue())

    def test_nested_help_shows_subcommand_without_loading_config(self):
        for words, expected in ((["app", "--help"], "build"),
                                (["xcb", "--help"], "ls"),
                                (["xcb", "ls", "--help"], "path")):
            with self.subTest(words=words), patch("sys.argv", ["q80", *words]), \
                 patch("q80.cli.load") as load, \
                 contextlib.redirect_stdout(io.StringIO()) as output:
                self.assertEqual(cli.main(), 0)
                load.assert_not_called()
                self.assertIn(expected, output.getvalue())

    def test_local_command_does_not_load_device_config(self):
        with patch("q80.cli.app_manifest.create") as create, \
             patch("q80.cli.load") as load:
            self.assertEqual(cli.main(["app", "new", "demo"]), 0)
        create.assert_called_once_with("demo")
        load.assert_not_called()

    def test_verbose_alone_starts_interactive_mode(self):
        with patch("q80.cli._repl", return_value=0) as repl, \
             patch("q80.cli.configure") as configure:
            self.assertEqual(cli.main(["--verbose"]), 0)
        repl.assert_called_once_with()
        configure.assert_called_once_with(True)

if __name__ == "__main__":
    unittest.main()
