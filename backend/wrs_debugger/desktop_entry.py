"""Frozen desktop entry point for the loopback-only WRS API server."""

import argparse
import os
import sys
from pathlib import Path

import uvicorn

from wrs_debugger.main import create_app
from wrs_debugger.settings import BackendSettings


def main() -> None:
    if hasattr(sys, "_MEIPASS") and "WRS_DDS_PROFILE" not in os.environ:
        os.environ["WRS_DDS_PROFILE"] = str(Path(sys._MEIPASS) / "dds" / "dds_profile.xml")
    parser = argparse.ArgumentParser(description="Start the WRS Debugger desktop backend.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", required=True, type=int)
    arguments = parser.parse_args()
    uvicorn.run(
        create_app(backend_settings=BackendSettings()),
        host=arguments.host,
        port=arguments.port,
    )


if __name__ == "__main__":
    main()
