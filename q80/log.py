"""Shared console formatting for Q80 operation logs."""

import logging
import subprocess
import sys


SUCCESS = 25
logging.addLevelName(SUCCESS, "SUCCESS")


class _Formatter(logging.Formatter):
    def format(self, record: logging.LogRecord) -> str:
        module = record.name.removeprefix("q80.").replace("transport.", "")
        symbol = {
            SUCCESS: "✅",
            logging.ERROR: "❌",
            logging.WARNING: "⚠️",
        }.get(record.levelno)
        prefix = f"[{module}]" + (f" {symbol}" if symbol else "")
        message = f"{prefix} {record.getMessage()}"
        if record.exc_info:
            message += "\n" + self.formatException(record.exc_info)
        return message


class _ConsoleHandler(logging.StreamHandler):
    pass


def configure(verbose: bool = False) -> None:
    logger = logging.getLogger("q80")
    handler = next((item for item in logger.handlers
                    if isinstance(item, _ConsoleHandler)), None)
    if handler is None:
        handler = _ConsoleHandler(sys.stderr)
        handler.setFormatter(_Formatter())
        logger.addHandler(handler)
    else:
        handler.setStream(sys.stderr)
    logger.setLevel(logging.DEBUG if verbose else logging.INFO)
    logger.propagate = False


def success(logger: logging.Logger, message: str, *args: object) -> None:
    logger.log(SUCCESS, message, *args)


def report_error(exc: Exception) -> None:
    module = "q80.cli"
    trace = exc.__traceback__
    while trace:
        name = trace.tb_frame.f_globals.get("__name__", "")
        if name.startswith("q80."):
            module = name
        trace = trace.tb_next
    logger = logging.getLogger(module)
    if isinstance(exc, subprocess.CalledProcessError):
        logger.error("External command failed (exit=%s)", exc.returncode)
    elif isinstance(exc, subprocess.TimeoutExpired):
        logger.error("External command timed out")
    else:
        logger.error("%s", exc)
        logger.debug("Full traceback", exc_info=exc)
