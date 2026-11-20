"""One-shot commands and an interactive prompt for the same operations."""

import argparse
from functools import lru_cache
import logging
import posixpath
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

from q80 import aip, app_manifest, apps, build, diagnostics, installer, mainapp, screenshot, windows_main
from q80.config import ROOT, load
from q80.log import configure, report_error, success
from q80.system import aip_root, ped, preload
from q80.transport.session import Session
from q80.transport.xcb import format_entry


log = logging.getLogger(__name__)


class _ArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        raise ValueError(message)


@lru_cache(maxsize=1)
def _parser() -> _ArgumentParser:
    parser = _ArgumentParser(prog="python -m q80", description="Q80 development and recovery tools")
    parser.add_argument("--verbose", action="store_true", help="show module debug logs")
    commands = parser.add_subparsers(dest="group", title="commands")

    commands.add_parser("doctor", help="show device status over Telnet")
    commands.add_parser("screenshot", help="save the Q80 screen as a PNG").add_argument("output", type=Path)
    shell = commands.add_parser("shell", help="run a Telnet command")
    shell.add_argument("command", nargs=argparse.REMAINDER, help="remote command")
    xcb = commands.add_parser(
        "xcb", help="XCB console, file commands, or native XCB commands",
        description="Run a file command, or enter the XCB console with no argument.",
        epilog="Other commands, such as devices and devinfo, go directly to xcb.exe.",
    )
    xcb_actions = xcb.add_subparsers(dest="action")
    xcb_actions.add_parser("check", help="probe the configured device and reconnect if needed")
    xcb_actions.add_parser("reconnect", help="disconnect and reconnect the configured device")
    xcb_actions.add_parser("ls", help="list remote files").add_argument("path", nargs="?", default="/")
    xcb_actions.add_parser("stat", help="show remote file details").add_argument("path")
    xcb_actions.add_parser("cat", help="print a remote file").add_argument("path")
    pull = xcb_actions.add_parser("pull", help="download a remote file")
    pull.add_argument("remote")
    pull.add_argument("local")
    push = xcb_actions.add_parser("push", help="upload a local file")
    push.add_argument("local")
    push.add_argument("remote")

    app = commands.add_parser("app", help="create, build, deploy, or run apps")
    app_actions = app.add_subparsers(dest="action", required=True)
    for action, description in (
        ("new", "create an app locally"), ("build", "build an app locally"),
        ("pack", "pack an app AIP locally"), ("deploy", "deploy an app over Telnet"),
        ("run", "run an app over Telnet"), ("stop", "stop an app over Telnet"),
        ("logs", "show app output over Telnet"),
    ):
        app_actions.add_parser(action, help=description).add_argument("name")
    app_actions.add_parser("list", help="list installed apps over Telnet")

    mainapp_parser = commands.add_parser("mainapp", help="build or deploy the app menu as MAINAPP")
    mainapp_actions = mainapp_parser.add_subparsers(dest="action", required=True)
    mainapp_actions.add_parser("build", help="build the MAINAPP menu library")
    mainapp_actions.add_parser("deploy", help="install the built MAINAPP menu over Telnet")

    exe = commands.add_parser("app-exe", help="build standalone main() apps on Windows")
    exe_actions = exe.add_subparsers(dest="action", required=True)
    exe_build = exe_actions.add_parser("build", help="compile a C source with main()")
    exe_build.add_argument("source", type=Path)
    exe_build.add_argument("output", type=Path)
    exe_build.add_argument("--lib", action="append", default=[], help="link a device library by name")

    aip_parser = commands.add_parser("aip", help="inspect or install AIP files")
    aip_actions = aip_parser.add_subparsers(dest="action", required=True)
    aip_actions.add_parser("inspect", help="inspect an AIP locally").add_argument("file", type=Path)
    aip_actions.add_parser("install", help="install an AIP over XCB").add_argument("file", type=Path)

    root = commands.add_parser("aip-root", help="temporary root access over XCB")
    root_actions = root.add_subparsers(dest="action", required=True)
    root_actions.add_parser("start", help="start temporary root access")
    root_actions.add_parser("exec", help="run one root command over XCB").add_argument("command", nargs=argparse.REMAINDER)
    root_actions.add_parser("stop", help="stop the root command channel")

    ped_parser = commands.add_parser("ped", help="PED status and current-boot bypass")
    ped_actions = ped_parser.add_subparsers(dest="action", required=True)
    ped_actions.add_parser("status", help="show PED status over Telnet")
    ped_actions.add_parser("bypass", help="handle PED for this boot")

    preload_parser = commands.add_parser("preload", help="persistent boot hook")
    preload_actions = preload_parser.add_subparsers(dest="action", required=True)
    preload_actions.add_parser("status", help="show boot hook status over Telnet")
    preload_actions.add_parser("install", help="install the boot hook")
    preload_actions.add_parser("restore", help="restore prior boot settings")

    commands.add_parser("help", help="show help for a command").add_argument("topic", nargs="?")
    return parser


def _words(line: str) -> list[str]:
    lexer = shlex.shlex(line, posix=True)
    lexer.whitespace_split = True
    lexer.escape = ""  # Windows paths retain backslashes.
    return list(lexer)


def _xcb_file(session: Session, action: str, args: list[str], current: str = "/") -> None:
    expected = {"ls": (0, 1), "stat": (1,), "cat": (1,), "pull": (2,), "push": (2,)}
    if len(args) not in expected[action]:
        raise ValueError(f"Invalid arguments for xcb {action}")
    path = args[1] if action == "push" else (args[0] if args else current)
    path = posixpath.normpath(path if path.startswith("/") else posixpath.join(current, path))
    if action == "ls":
        for name, mode, size in session.xcb.ls(path):
            print(format_entry(name, mode, size))
    elif action == "stat":
        mode, size, mtime = session.xcb.stat(path)
        print(f"mode={mode:o} size={size} mtime={mtime} {path}")
    elif action == "cat":
        with tempfile.TemporaryDirectory(prefix="q80-xcb-") as temp:
            local = Path(temp) / "content"
            session.xcb.pull(path, local)
            sys.stdout.buffer.write(local.read_bytes())
            print()
    elif action == "pull":
        session.xcb.pull(path, Path(args[1]))
        success(logging.getLogger("q80.transport.xcb"), "Saved %s", args[1])
    else:
        session.xcb.push(Path(args[0]), path)


def _browse(session: Session) -> None:
    current = "/"
    logging.getLogger("q80.transport.xcb").info(
        "Console: check, reconnect, pwd, cd, ls, stat, cat, pull, push; other commands go to xcb.exe"
    )
    while True:
        try:
            words = _words(input(f"xcb:{current}> "))
        except (EOFError, KeyboardInterrupt):
            print()
            return
        if not words:
            continue
        action, *args = words
        if action in ("quit", "exit"):
            return
        if action == "help":
            if args == ["xcb"]:
                session.xcb.raw("help")
            else:
                logging.getLogger("q80.transport.xcb").info(
                    "check, reconnect, pwd, cd [path], ls [path], stat path, cat path, pull path local, push local path"
                )
                logging.getLogger("q80.transport.xcb").info(
                    "Commands such as devinfo and devices go to xcb.exe"
                )
            continue
        if action == "check" and not args:
            try:
                session.xcb.connect()
            except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
                report_error(exc)
            continue
        if action == "reconnect" and not args:
            try:
                session.xcb.reconnect()
            except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
                report_error(exc)
            continue
        if action == "pwd" and not args:
            print(current)
            continue
        if action == "cd" and len(args) <= 1:
            path = args[0] if args else "/"
            current = posixpath.normpath(path if path.startswith("/") else posixpath.join(current, path))
            continue
        if action in ("ls", "stat", "cat", "pull", "push"):
            try:
                _xcb_file(session, action, args, current)
            except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as exc:
                report_error(exc)
            continue
        try:
            session.xcb.raw(action, *args)
        except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
            report_error(exc)


def _device_session(session: Session | None) -> Session:
    return session if session is not None else Session(load())


def execute(words: list[str], session: Session | None) -> int | None:
    parser = _parser()
    if not words:
        parser.print_help()
        return 0
    if words[0] == "--verbose":
        configure(True)
        words = words[1:]
    xcb_actions = {"check", "reconnect", "ls", "stat", "cat", "pull", "push"}
    if words and words[0] == "xcb" and len(words) > 1 and words[1] not in xcb_actions | {"-h", "--help"}:
        session = _device_session(session)
        action, *args = words[1:]
        return session.xcb.raw(action, *args).returncode
    if words and words[0] == "shell" and len(words) > 1 and words[1] not in ("-h", "--help"):
        session = _device_session(session)
        result = session.telnet.run(" ".join(words[1:]), check=False)
        print(result.output, end="")
        telnet_log = logging.getLogger("q80.transport.telnet")
        if result.code:
            telnet_log.error("Command failed (exit=%s)", result.code)
        else:
            success(telnet_log, "Command OK (exit=0)")
        return result.code
    if words[:2] == ["aip-root", "exec"] and len(words) > 2 and words[2] not in ("-h", "--help"):
        session = _device_session(session)
        print(aip_root.run(session, " ".join(words[2:])), end="")
        return 0

    try:
        args = parser.parse_args(words)
    except SystemExit as exc:
        return int(exc.code)
    if args.group is None:
        parser.print_help()
        return 0
    if args.group == "help":
        if not args.topic:
            parser.print_help()
            return 0
        try:
            parser.parse_args([args.topic, "--help"])
        except SystemExit as exc:
            return int(exc.code)
        return 0

    if args.group == "app" and args.action == "new":
        app_manifest.create(args.name)
        return 0
    if args.group == "app-exe":
        windows_main.build(args.source, args.output, load(), tuple(args.lib))
        return 0
    if args.group == "mainapp" and args.action == "build":
        mainapp.build()
        return 0
    if args.group == "app" and args.action == "pack":
        app = app_manifest.load(args.name)
        aip.pack_app(app, ROOT / "build" / f"{app.app_id}.aip")
        return 0
    if args.group == "aip" and args.action == "inspect":
        for entry, size in aip.inspect(args.file):
            print(f"{size:>10} {entry}")
        return 0
    if args.group == "shell" and not args.command:
        raise ValueError("Missing shell command")
    if args.group == "aip-root" and args.action == "exec" and not args.command:
        raise ValueError("Missing root command")

    session = _device_session(session)
    if args.group == "doctor":
        print(diagnostics.doctor(session), end="")
    elif args.group == "screenshot":
        screenshot.capture(session, args.output)
    elif args.group == "xcb":
        if args.action is None:
            _browse(session)
        elif args.action == "check":
            session.xcb.connect()
        elif args.action == "reconnect":
            session.xcb.reconnect()
        else:
            session.xcb.connect()
            if args.action in ("ls", "stat", "cat"):
                paths = [args.path]
            elif args.action == "push":
                paths = [args.local, args.remote]
            else:
                paths = [args.remote, args.local]
            _xcb_file(session, args.action, paths)
    elif args.group == "ped":
        if args.action == "status":
            print(diagnostics.ped_status(session), end="")
        else:
            print(ped.bypass(session), end="")
    elif args.group == "preload":
        if args.action == "status":
            print(diagnostics.preload_status(session), end="")
        elif args.action == "install":
            print(preload.install(session))
        else:
            print(preload.restore(session))
    elif args.group == "aip-root":
        if args.action == "start":
            aip_root.start(session)
        else:
            aip_root.stop(session)
    elif args.group == "mainapp":
        mainapp.deploy(session)
    elif args.group == "app":
        if args.action == "list":
            print(diagnostics.installed_apps(session), end="")
        else:
            app = app_manifest.load(args.name)
            if args.action == "build":
                build.build(app, session.config)
            elif args.action == "deploy":
                apps.deploy(session, app)
            elif args.action == "run":
                print(apps.run(session, app), end="")
            elif args.action == "stop":
                print(apps.stop(session, app), end="")
            elif args.action == "logs":
                print(apps.logs(session, app), end="")
    elif args.group == "aip":
        installer.install(session, args.file)
    return 0


def _repl() -> int:
    session = Session(load())
    log.info("Type help for commands or quit to exit")
    while True:
        try:
            words = _words(input("> "))
            if words and words[0] in ("quit", "exit"):
                return 0
            execute(words, session)
        except (EOFError, KeyboardInterrupt):
            print()
            return 0
        except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as exc:
            report_error(exc)


def main(argv: list[str] | None = None) -> int:
    words = list(sys.argv[1:] if argv is None else argv)
    verbose = words[:1] == ["--verbose"]
    configure(verbose)
    if verbose:
        words = words[1:]
    try:
        return (execute(words, None) or 0) if words else _repl()
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as exc:
        report_error(exc)
        return 1
