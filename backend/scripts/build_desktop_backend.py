"""Create the self-contained Linux desktop backend with PyInstaller."""

import subprocess
import sys
from pathlib import Path

from PyInstaller.__main__ import run

ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = ROOT.parent


def build_native_module() -> None:
    output = ROOT
    build_dir = ROOT / "build" / "native"
    pybind11_dir = subprocess.check_output(
        [sys.executable, "-m", "pybind11", "--cmakedir"], text=True
    ).strip()
    subprocess.run(
        [
            "cmake",
            "-S",
            str(PROJECT_ROOT),
            "-B",
            str(build_dir),
            "-DBUILD_TRANSMITTER_PYTHON=ON",
            "-DBUILD_TESTING=OFF",
            f"-Dpybind11_DIR={pybind11_dir}",
            f"-DPython_EXECUTABLE={sys.executable}",
            f"-DWRS_PYTHON_OUTPUT_DIR={output}",
        ],
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", "wrs_debugger_native"], check=True
    )


build_native_module()

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
        "--hidden-import",
        "wrs_debugger_native",
        "--paths",
        str(ROOT),
        str(ROOT / "wrs_debugger" / "desktop_entry.py"),
    ]
)
