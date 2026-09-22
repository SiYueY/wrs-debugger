"""Create the self-contained Linux desktop backend with PyInstaller."""

from pathlib import Path

from PyInstaller.__main__ import run

ROOT = Path(__file__).resolve().parents[1]

run(
    [
        "--noconfirm",
        "--clean",
        "--onedir",
        "--name",
        "wrs-debugger-backend",
        "--distpath",
        str(ROOT / "dist"),
        "--workpath",
        str(ROOT / "build"),
        "--specpath",
        str(ROOT / "build"),
        "--collect-all",
        "uvicorn",
        "--collect-all",
        "fastapi",
        "--collect-all",
        "pydantic",
        "--collect-all",
        "pydantic_core",
        "--collect-all",
        "starlette",
        "--collect-all",
        "anyio",
        "--collect-submodules",
        "wrs_debugger",
        "--paths",
        str(ROOT),
        str(ROOT / "wrs_debugger" / "desktop_entry.py"),
    ]
)
