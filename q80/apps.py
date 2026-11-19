"""Daily app deployment and process control over the network shell."""

import logging
from pathlib import Path
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
    success(log, "App deployed: %s", app.app_id)


def run(session: Session, app: App) -> str:
    log.info("Run app: %s", app.app_id)
    app_id = quote(app.app_id)
    tm_path = quote(f"/tmp/{app.app_id}.tm")
    stdout = quote(f"/tmp/{app.app_id}.stdout")
    command = (
        "TM=$(pidof tm); "
        f"if [ -n \"$TM\" ]; then kill -STOP $TM; echo \"$TM\" > {tm_path}; fi; "
        f"setsid /usr/bin/runapp {app_id} </dev/null >{stdout} 2>&1 & START=$!; "
        "sleep 1; P=; for C in $(pidof runapp); do "
        f"if grep -aq {app_id} /proc/$C/cmdline; then P=$C; fi; done; "
        f"if [ -n \"$P\" ]; then echo APP_PID=$P; "
        f"else wait $START; RC=$?; TM=$(cat {tm_path} 2>/dev/null); "
        "if [ -n \"$TM\" ]; then kill -CONT $TM; fi; "
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
        f"TM=$(cat {tm_path} 2>/dev/null); "
        "if [ -n \"$TM\" ]; then kill -CONT $TM; fi; "
        f"rm -f {tm_path}; echo APP_STOPPED"
    )
    output = session.telnet.run(command).output
    success(log, "App stop command complete: %s", app.app_id)
    return output


def logs(session: Session, app: App) -> str:
    log.info("Read app output: %s", app.app_id)
    path = quote(f"/tmp/{app.app_id}.stdout")
    return session.telnet.run(f"cat {path}", check=False).output
