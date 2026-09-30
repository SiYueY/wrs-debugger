"""Backend logging configuration for Python and native WRS modules."""

import logging
import os
import sys
from logging.handlers import RotatingFileHandler
from pathlib import Path

from uvicorn.logging import DefaultFormatter


class TerminalFormatter(DefaultFormatter):
    """Use Uvicorn's level-prefix colors with a WRS or backend source prefix."""

    def __init__(self, prefix: str, *, use_colors: bool) -> None:
        super().__init__(
            f"{prefix} %(asctime)s %(levelprefix)s %(message)s",
            use_colors=use_colors,
        )


def _log_directory() -> Path:
    configured = os.environ.get("WRS_DEBUGGER_LOG_DIR")
    if configured:
        return Path(configured)
    state_home = Path(os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state"))
    return state_home / "wrs-debugger" / "logs"


def _log_level() -> int:
    return getattr(logging, os.environ.get("WRS_DEBUGGER_LOG_LEVEL", "INFO").upper(), logging.INFO)


def _console_formatter(prefix: str, handler: logging.Handler) -> TerminalFormatter:
    stream = getattr(handler, "stream", sys.stderr)
    return TerminalFormatter(prefix, use_colors=bool(getattr(stream, "isatty", lambda: False)()))


def configure_logging() -> None:
    """Configure colored terminal and plain rotating-file handlers for all application logs."""
    level = _log_level()
    try:
        directory = _log_directory()
        directory.mkdir(parents=True, exist_ok=True)
    except OSError:
        directory = None

    for name, prefix in (
        ("wrs_logging", "[wrs]"),
        ("wrs_debugger", "[backend]"),
        ("uvicorn", "[backend]"),
        ("uvicorn.error", "[backend]"),
        ("uvicorn.access", "[backend]"),
    ):
        logger = logging.getLogger(name)
        file_formatter = logging.Formatter(f"{prefix} %(asctime)s %(levelname)s %(message)s")
        logger.setLevel(level)
        for handler in logger.handlers:
            if getattr(handler, "_wrs_file_handler", False):
                handler.setFormatter(file_formatter)
            else:
                handler.setFormatter(_console_formatter(prefix, handler))
        if directory is not None and not any(
            getattr(handler, "_wrs_file_handler", False) for handler in logger.handlers
        ):
            handler = RotatingFileHandler(
                directory / "backend.log",
                maxBytes=10 * 1024 * 1024,
                backupCount=5,
                encoding="utf-8",
            )
            handler._wrs_file_handler = True  # type: ignore[attr-defined]
            handler.setFormatter(file_formatter)
            logger.addHandler(handler)
        if name in {"wrs_logging", "wrs_debugger"} and not any(
            not getattr(handler, "_wrs_file_handler", False) for handler in logger.handlers
        ):
            console = logging.StreamHandler()
            console.setFormatter(_console_formatter(prefix, console))
            logger.addHandler(console)
        logger.propagate = False
