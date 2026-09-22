"""Frozen desktop entry point for the loopback-only WRS API server."""

import argparse

import uvicorn

from wrs_debugger.main import app


def main() -> None:
    parser = argparse.ArgumentParser(description="Start the WRS Debugger desktop backend.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", required=True, type=int)
    arguments = parser.parse_args()
    uvicorn.run(app, host=arguments.host, port=arguments.port)


if __name__ == "__main__":
    main()
