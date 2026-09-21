# Repository Guidelines

## Project Structure & Boundaries

`frontend/web/` contains the single Vue 3 business UI: routes in `src/router`, Pinia
stores in `src/stores`, typed API adapters in `src/api`, and pages in `src/views`.
`frontend/desktop/` is the Electron main/preload shell only; it must not duplicate
renderer pages or device logic. `backend/wrs_debugger/` holds FastAPI routes,
Pydantic contracts, services, and WebSocket events; backend tests are in
`backend/tests/`. Native serial code lives in `wrs/include/` and `wrs/src/`.
Documentation belongs in `docs/`.

Preserve the business boundary: UI code calls typed adapters, backend routes express
product operations, and hardware/protocol details stay behind the native/service
layer. Do not expose serial frames, CRC details, or device paths through UI APIs.

## Development Commands

Run in the owning workspace:

```bash
cd frontend && pnpm install && pnpm dev:web       # Vite at localhost:5173
cd frontend && pnpm dev:desktop                    # Vite plus Electron shell
cd frontend && pnpm typecheck && pnpm format:check
cd frontend && pnpm build:desktop                  # production artifacts
cd backend && uv sync && uv run pytest             # API tests
cd backend && uv run ruff format . && uv run ruff check .
cd backend && uv run mypy wrs_debugger
```

Use Node 22/pnpm 11 and Python 3.12. When backend dependencies change, update and
commit `uv.lock`. The native module has no checked-in build target yet; add one with
its tests rather than documenting an ad-hoc compile command.

## Style & Naming

Follow `.editorconfig`: two spaces for TypeScript/Vue, four for Python and C++.
Prettier uses 100 columns, single quotes, and trailing commas. Use Vue Composition
API with `<script setup lang="ts">`; name components/views in PascalCase and
TypeScript files, stores, and API modules in the existing camel/lowercase pattern.
Keep Python fully typed and compatible with strict mypy; use `snake_case` for Python
fields and REST JSON. C++ follows its existing four-space style, `namespace serial`,
and `PascalCase` public types.

## Tests, Commits & Pull Requests

Add focused `pytest` tests named `test_<behavior>` for backend changes, including
success, validation, and conflicting-operation paths. Test UI state/API seams when
they change; do not require real hardware for unit tests. Run the relevant checks
above before review.

Use short, imperative commit subjects; existing history uses concise Chinese subjects
such as `新增串口模块`. Keep commits scoped. Pull requests should describe the user or
protocol impact, list validation commands, link the issue when available, and include
screenshots for visible UI changes. Call out any safety, serial-protocol, or API
contract change explicitly.
