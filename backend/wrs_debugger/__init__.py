"""WRS Debugger backend application."""

from importlib.metadata import PackageNotFoundError, version

try:
    __version__ = version("wrs-debugger-backend")
except PackageNotFoundError:
    __version__ = "0.1.0"

__all__ = ["__version__"]
