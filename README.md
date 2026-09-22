# wrs-debugger

Wireless Remote Stop Debugger.

## Linux desktop release

The complete AppImage release is built by the project-level script rather than a
frontend package-manager command:

```bash
./scripts/package-desktop-appimage.sh
```

The build machine needs Node.js 22, pnpm 11 and uv. The resulting AppImage is
self-contained and does not require Python, uv or backend Python dependencies
on the target computer.
