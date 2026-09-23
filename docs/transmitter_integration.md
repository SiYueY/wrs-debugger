# 发射器联调

Simulator 与 Debugger 是两个独立应用。先构建并启动 Simulator：

```bash
cmake -S simulator/transmitter_simulator -B build/simulator -DBUILD_GUI=OFF -DBUILD_TESTING=OFF
cmake --build build/simulator
build/simulator/transmitter_simulator --headless
```

构建 native Backend 后，以 native 模式启动：

```bash
cd backend
uv sync --group desktop
uv run --group desktop python scripts/build_desktop_backend.py
uv run python -m wrs_debugger.desktop_entry --port 8000
```

启动调试工具后刷新串口列表。Simulator 的 `/dev/pts/N` 与真实 USB 串口会出现在同一个下拉列表；选择任一串口时，均由同一 C++ 协议握手确认是否为发射器。Simulator 可独立停止或用其 GUI 注入 CRC、超时、错误事务号和断连故障，Debugger 不会管理 Simulator 生命周期。
