#!/usr/bin/env bash

# Build the complete self-contained Linux desktop release from the project root.
set -euo pipefail

project_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
frontend_dir="$project_root/frontend"
backend_dir="$project_root/backend"
desktop_dir="$frontend_dir/desktop"
release_dir="$desktop_dir/release"
distribution_dir="$project_root/dist"
desktop_package="$desktop_dir/package.json"

if [[ "${1:-}" == "--help" ]]; then
  printf 'Usage: %s\n' "${BASH_SOURCE[0]}"
  printf 'Build the complete self-contained WRS Debugger Linux AppImage.\n'
  exit 0
fi

if (( $# != 0 )); then
  printf 'Unknown argument: %s\n' "$1" >&2
  exit 2
fi

command -v pnpm >/dev/null || { echo "pnpm is required to build the frontend." >&2; exit 1; }
command -v uv >/dev/null || { echo "uv is required to freeze the backend." >&2; exit 1; }
command -v node >/dev/null || { echo "Node.js is required to read the desktop package version." >&2; exit 1; }

version=$(node -e 'process.stdout.write(require(process.argv[1]).version)' "$desktop_package")
build_date=$(date +%Y%m%d)
artifact_name="WRS-Debugger-${version}-${build_date}.AppImage"
release_artifact="$release_dir/$artifact_name"
published_artifact="$distribution_dir/$artifact_name"

mkdir -p "$release_dir" "$distribution_dir"
# electron-builder always emits its default product-name artifact.  Remove only
# prior AppImage outputs from this generated staging directory so the result is
# unambiguous before renaming it to the release filename below.
find "$release_dir" -maxdepth 1 -type f -name '*.AppImage' -delete

pnpm --dir "$frontend_dir" build:web
(
  cd "$backend_dir"
  uv run --group desktop python scripts/build_desktop_backend.py
)
pnpm --dir "$frontend_dir" --filter @wrs-debugger/desktop build
pnpm --dir "$frontend_dir" --filter @wrs-debugger/desktop package

mapfile -t generated_artifacts < <(find "$release_dir" -maxdepth 1 -type f -name '*.AppImage' -print)
if (( ${#generated_artifacts[@]} != 1 )); then
  printf 'Expected exactly one AppImage in %s; found %d.\n' "$release_dir" "${#generated_artifacts[@]}" >&2
  exit 1
fi

mv -f "${generated_artifacts[0]}" "$release_artifact"
chmod +x "$release_artifact"
cp -f "$release_artifact" "$published_artifact"
chmod +x "$published_artifact"

printf 'Build copy: %s\n' "$release_artifact"
printf 'AppImage created: %s\n' "$published_artifact"
