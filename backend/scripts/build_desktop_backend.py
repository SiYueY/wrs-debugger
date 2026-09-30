"""Create the self-contained Linux desktop backend with PyInstaller."""

import os
import platform
import subprocess
import sys
from pathlib import Path

from PyInstaller.__main__ import run

ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = ROOT.parent


def build_native_module() -> None:
    output = ROOT
    build_dir = ROOT / "build" / "wrs_native"
    pybind11_dir = subprocess.check_output(
        [sys.executable, "-m", "pybind11", "--cmakedir"], text=True
    ).strip()
    driver_root = Path(
        os.environ.get(
            "HUMANOID_DRIVER_ROOT",
            PROJECT_ROOT.parents[2] / "humanoid-driver" / "humanoid-driver",
        )
    )
    generic_bin = os.environ.get(
        "GENERIC_BIN_DIR",
        "linux/x86_64/x64" if platform.machine() == "x86_64" else "linux/arm/aarch64",
    )
    subprocess.run(
        [
            "cmake",
            "-S",
            str(PROJECT_ROOT / "wrs"),
            "-B",
            str(build_dir),
            "-DBUILD_TRANSMITTER_PYTHON=ON",
            "-DBUILD_TESTING=OFF",
            f"-DHUMANOID_DRIVER_ROOT={driver_root}",
            f"-DGENERIC_BIN_DIR={generic_bin}",
            f"-Dpybind11_DIR={pybind11_dir}",
            f"-DPython_EXECUTABLE={sys.executable}",
            f"-DWRS_PYTHON_OUTPUT_DIR={output}",
        ],
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", "wrs_debugger_adapter"], check=True
    )


build_native_module()

driver_root = Path(
    os.environ.get(
        "HUMANOID_DRIVER_ROOT", PROJECT_ROOT.parents[2] / "humanoid-driver" / "humanoid-driver"
    )
)
generic_bin = os.environ.get(
    "GENERIC_BIN_DIR",
    "linux/x86_64/x64" if platform.machine() == "x86_64" else "linux/arm/aarch64",
)
dds_libraries = [
    driver_root / "thirdparty" / "dds_wrapper" / "lib" / generic_bin / "libdds_wrapper.so",
    *(driver_root / "thirdparty" / "fastdds" / "lib" / generic_bin).glob("*.so*"),
]


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
        "wrs_debugger_adapter",
        "--add-data",
        f"{PROJECT_ROOT / 'wrs' / 'tests' / 'data' / 'dds_profile.xml'}:dds",
        *[argument for library in dds_libraries for argument in ("--add-binary", f"{library}:.")],
        "--paths",
        str(ROOT),
        str(ROOT / "wrs_debugger" / "desktop_entry.py"),
    ]
)
