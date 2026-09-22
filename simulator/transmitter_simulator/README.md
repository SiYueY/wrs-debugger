# WRS Transmitter Simulator

An independent Linux PTY implementation of the transmitter-side USB Serial protocol. It does not
include or link any `wrs` source; `docs/wrs/serial_protocol.md` is the wire-contract authority.

## Build and test

Headless CI build:

```bash
cmake -S simulator/transmitter_simulator -B build/transmitter-simulator \
  -DBUILD_TESTING=ON -DBUILD_GUI=OFF
cmake --build build/transmitter-simulator -j
ctest --test-dir build/transmitter-simulator --output-on-failure
```

GUI build (requires Linux OpenGL/X11 development packages):

```bash
cmake -S simulator/transmitter_simulator -B build/transmitter-simulator-gui
cmake --build build/transmitter-simulator-gui -j
```

## Run

```bash
./build/transmitter-simulator/transmitter_simulator --headless
```

The ready line contains the stable PTY path. Stop with `SIGINT` or `SIGTERM`; the stable link is
removed only when it still points at the simulator-owned PTY.

In GUI mode, open **Device** and enter an absolute path under **Stable virtual serial path**. Use
**Apply & Reconnect** to publish a new PTY path. The existing Host is disconnected only after the
new path, lock, and PTY have been created successfully; the setting lasts for the current process
only. `--pty-link` remains available for Headless automation.

## Third party

Build-time fetched, pinned releases (stored under the CMake build directory, not this repository):

| Project | Version |
| --- | --- |
| Dear ImGui | v1.91.9b |
| GLFW | 3.3.9 |
| Catch2 | v2.13.10 |

Their upstream licenses are retained in the fetched source trees.
