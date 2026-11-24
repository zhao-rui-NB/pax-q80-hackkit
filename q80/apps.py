"""Daily app deployment and process control over the network shell."""

import logging
from pathlib import Path
import stat
import tempfile

from q80.app_manifest import App
from q80.log import success
from q80.transport.session import Session
from q80.transport.telnet import quote


APP_UID = 999
APP_GID = 20003
log = logging.getLogger(__name__)


def deploy(session: Session, app: App) -> None:
    log.info("Deploy app: %s", app.app_id)
    if not app.output.is_file():
        raise FileNotFoundError(f"Build output missing: {app.output}")
    app_dir = f"/data/app/{app.app_id}"
    info_dir = f"/data/appinfo/{app.app_id}"
    shell = session.telnet
    shell.run(f"mkdir -p {quote(app_dir)} {quote(info_dir)}")
    shell.push(app.output, f"{app_dir}/{app.binary}", 0o750)
    with tempfile.TemporaryDirectory(prefix="q80-app-") as temp:
        info = Path(temp) / "appinfo"
        info.write_bytes(app.appinfo)
        shell.push(info, f"{info_dir}/appinfo", 0o664)
    shell.run(
        f"chown {APP_UID}:{APP_GID} {quote(app_dir)} {quote(info_dir)} "
        f"{quote(app_dir + '/' + app.binary)} {quote(info_dir + '/appinfo')}"
    )
    runtime = app.folder / "runtime"
    if runtime.is_dir():
        files = sorted(path for path in runtime.rglob("*") if path.is_file())
        directories = sorted({path.parent.relative_to(runtime) for path in files},
                             key=lambda path: (len(path.parts), str(path)))
        for relative in directories:
            if relative.parts:
                shell.run(f"mkdir -p {quote(app_dir + '/' + relative.as_posix())}")
        session.xcb.connect()
        for path in files:
            remote = app_dir + "/" + path.relative_to(runtime).as_posix()
            session.xcb.push(path, remote, stat.S_IFREG | 0o644)
            shell.run(f"chown {APP_UID}:{APP_GID} {quote(remote)}")
        for relative in directories:
            if relative.parts:
                shell.run(f"chown {APP_UID}:{APP_GID} "
                          f"{quote(app_dir + '/' + relative.as_posix())}")
    success(log, "App deployed: %s", app.app_id)


def run(session: Session, app: App) -> str:
    log.info("Run app: %s", app.app_id)
    app_id = quote(app.app_id)
    tm_path = quote(f"/tmp/{app.app_id}.tm")
    stdout = quote(f"/tmp/{app.app_id}.stdout")
    # On this Q80, the first tm restart after XUI exits draws only its header.
    # keyman starts tm again after each exit; the second restart draws the menu.
    watcher = (
        f"/usr/bin/runapp {app_id}; RC=$?; "
        f"TM=$(cat {tm_path} 2>/dev/null); "
        'if [ -n "$TM" ]; then '
        'kill -CONT $TM; kill -TERM $TM; '
        'sleep 2; NEW=$(pidof tm); '
        'if [ -n "$NEW" ] && [ "$NEW" != "$TM" ]; then kill -TERM $NEW; fi; '
        'fi; '
        f"rm -f {tm_path}; exit $RC"
    )
    command = (
        "TM=$(pidof tm); "
        f"if [ -n \"$TM\" ]; then kill -STOP $TM; echo \"$TM\" > {tm_path}; fi; "
        f"setsid sh -c {quote(watcher)} </dev/null >{stdout} 2>&1 & START=$!; "
        "sleep 1; P=; for C in $(pidof runapp); do "
        f"if grep -aq {app_id} /proc/$C/cmdline; then P=$C; fi; done; "
        f"if [ -n \"$P\" ]; then echo APP_PID=$P; "
        f"else wait $START; RC=$?; TM=$(cat {tm_path} 2>/dev/null); "
        "if [ -n \"$TM\" ]; then kill -CONT $TM; kill -TERM $TM; "
        f"rm -f {tm_path}; fi; "
        "echo APP_EXIT_CODE=$RC; fi"
    )
    output = session.telnet.run(command).output
    success(log, "App run command complete: %s", app.app_id)
    return output


def stop(session: Session, app: App) -> str:
    log.info("Stop app: %s", app.app_id)
    tm_path = quote(f"/tmp/{app.app_id}.tm")
    app_id = quote(app.app_id)
    command = (
        "for P in $(pidof runapp); do "
        f"if grep -aq {app_id} /proc/$P/cmdline; then kill -TERM $P; fi; done; "
        "sleep 3; "
        f"TM=$(cat {tm_path} 2>/dev/null); "
        "if [ -n \"$TM\" ]; then kill -CONT $TM; kill -TERM $TM; fi; "
        f"rm -f {tm_path}; echo APP_STOPPED"
    )
    output = session.telnet.run(command).output
    success(log, "App stop command complete: %s", app.app_id)
    return output


def logs(session: Session, app: App) -> str:
    log.info("Read app output: %s", app.app_id)
    path = quote(f"/tmp/{app.app_id}.stdout")
    return session.telnet.run(f"cat {path}", check=False).output
