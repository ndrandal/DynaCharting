# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DynaCharting is a high-performance real-time charting engine intended as an embeddable library for internal use. The long-term target is C++ doing the heavy lifting for both data processing and rendering. The TypeScript/WebGL2 frontend served as a prototype to prove out concepts (scene graph, pipelines, data ingestion) and will be progressively replaced as the C++ core gains rendering capability.

**Current milestone:** First render from the C++ side (D2.1 — achieved).

## Repository Layout

- **`core/`** — C++17 static libraries (`dc` and `dc_gl`). Scene graph, command processor, resource registry, pipeline catalog, and GL rendering backend. This is where most new work happens. Built with CMake.
  - `dc` — Pure C++ core (no GL deps). Scene graph, commands, pipelines.
  - `dc_gl` — OpenGL rendering backend. Links against `dc`, GLAD, and OSMesa. Contains `GlContext`, `OsMesaContext`, `ShaderProgram`, `GpuBufferManager`, `Renderer`.
  - `dc_gpu` — WebGPU/Dawn rendering backend (scaffold, OFF by default). Built only with `-DDC_FETCH_DAWN=ON`; links `dc` and `dawn::webgpu_dawn`. See the WebGPU/Dawn section under Build & Development Commands.
- **`packages/engine-host/`** — TypeScript WebGL2 rendering host (`@repo/engine-host`). Prototype renderer — reference implementation for pipeline specs, glyph atlas, data ingestion.
- **`packages/chart-controller/`** — High-level chart API (`@repo/chart-controller`). Recipe lifecycle and transform management. Depends on engine-host.
- **`apps/demos/hello-engine/`** — Vite demo app. Useful as a reference for how the pieces connect.

## Build & Development Commands

### TypeScript (pnpm workspace)

```bash
pnpm install                                        # install all workspace deps
pnpm --filter @repo/demo-hello-engine dev           # run Vite dev server (port 5173)
```

### C++ Core (CMake)

```bash
cmake -B build -DTHIRD_PARTY_ROOT=./third_party     # configure (RapidJSON required)
cmake --build build                                  # build library + tests
ctest --test-dir build                               # run all C++ tests
ctest --test-dir build -R dc_d1_1_smoke              # run a single test by name
```

CMake options: `DC_BUILD_TESTS` (default ON), `DC_WARNINGS_AS_ERRORS` (default OFF), `DC_FETCH_DAWN` (default OFF — see below).

**Note:** Third-party deps (RapidJSON, GLAD) need to be present under `./third_party` (or path set via `-DTHIRD_PARTY_ROOT`). OSMesa (`libosmesa6-dev`) must be installed for `dc_gl` and the D2.1 render test. If OSMesa is not found, `dc_gl` is gracefully skipped.

### WebGPU / Dawn backend (`dc_gpu`)

The WebGPU/Dawn rendering backend (`dc_gpu`) is **OFF by default**. Normal `dc` / `dc_gl` builds and CI for other tickets are completely unaffected unless you explicitly opt in.

Enable Dawn (fetches and builds it from source via CMake `FetchContent`):

```bash
cmake -B build-dawn -DTHIRD_PARTY_ROOT=./third_party -DDC_FETCH_DAWN=ON
cmake --build build-dawn --target dc_gpu -j$(nproc)
```

- **Pinned Dawn revision:** commit `58263faefe3c52fac4656825c6d55f85ee3c7536` — the immutable tip of branch `chromium/7880` as of **2026-06-09**. We pin an explicit commit hash (never a moving branch) for reproducibility. Update this hash deliberately when bumping Dawn.
- **Source:** `https://dawn.googlesource.com/dawn`. Dawn's own dependencies are fetched with its `fetch_dawn_dependencies.py` helper (`DAWN_FETCH_DEPENDENCIES=ON`), so `depot_tools` is **not** required.
- **Build cost (heads-up):** build-from-source is **slow** — the first configure clones ~3-4 GB of Dawn + its third-party deps, and a full compile takes **30-60+ minutes** and needs `python3` and `ninja`. Subsequent incremental builds are fast. The full Dawn build is validated in CI (ENC-499).
- **Lean build:** Dawn samples, tests, benchmarks, fuzzers, node bindings, install rules, and Tint command-line tools/tests are all disabled. Dawn is built as a single monolithic static library.
- **Linked target:** `dc_gpu` links the Dawn monolithic WebGPU target `dawn::webgpu_dawn` (alias of `webgpu_dawn`) plus `dc`. When `DC_FETCH_DAWN=OFF` (or Dawn is unavailable), `DC_HAS_DAWN` is FALSE and `dc_gpu` is gracefully skipped — exactly like the OSMesa/GLFW/`dc_gl` skip behavior.
- `dc_gpu` currently contains only a placeholder translation unit (`core/src/gpu/placeholder.cpp`) and a public header (`core/include/gpu/Gpu.hpp`); the real backend lands in later P0/P1 tickets.

## Architecture

### Target Data Flow (C++ core)

The C++ core owns the scene graph and will own rendering. The intended flow is:
```
Data Source → C++ Processing → Scene Graph → C++ Rendering
```

### Current Prototype Data Flow (TypeScript)

The TypeScript side demonstrates the target architecture in the browser:
```
Worker (ingest.worker.ts)  →  ArrayBuffer batches  →  Main Thread Queue
  →  CoreIngestStub (binary parsing)  →  GPU Buffer Sync  →  WebGL Draw Calls
```

### Binary Ingestion Format (per record)

`[1B op] [4B bufferId (u32 LE)] [4B offsetBytes (u32 LE)] [4B payloadBytes (u32 LE)] [payload]`

Op codes: 1 = append, 2 = updateRange.

### Rendering Pipelines

Pipeline types (defined in TS prototype at `packages/engine-host/src/pipelines.ts`, to be ported to C++):
- `triSolid@1`, `line2d@1`, `points@1` — vertex-based
- `instancedRect@1`, `instancedCandle@1` — instanced geometry (bars, OHLC)
- `textSDF@1` — SDF text rendering with glyph atlas

### Key Subsystems

- **Transform System (D1.5)** — Affine 2D transforms (column-major mat3) on DrawItems for pan/zoom.
- **Buffer Cache Policy (D6.1)** — Per-buffer byte cap with ring-buffer eviction (`evictFront`, `keepLast`).
- **Recipe System (D7.1/D8.1)** — Declarative chart definitions with deterministic ID allocation. Create/dispose command pairs.
- **Text SDF (D2.3)** — Glyph atlas with shelf packing and SDF generation via distance transform.
- **WebSocket Support (D5.2)** — Worker connects to real server or falls back to fake streams for demos.

### C++ Core Classes

- `Scene` — Owns panes, layers, draw items. Cascading delete, frame atomicity (beginFrame/commitFrame).
- `CommandProcessor` — Parses JSON commands, applies them to the Scene.
- `ResourceRegistry` — Manages GPU resource metadata.
- `PipelineCatalog` — Registers and resolves pipeline types.

### C++ GL Classes (`dc_gl`)

- `GlContext` — Abstract interface for GL context creation (`init`, `swapBuffers`, `readPixels`).
- `OsMesaContext` — Headless OSMesa implementation of `GlContext`. Creates GL 3.3 core profile context, loads GLAD via `OSMesaGetProcAddress`.
- `ShaderProgram` — Shader compile/link wrapper with uniform setters.
- `GpuBufferManager` — Stores CPU bytes per buffer ID, manages VBO upload. Connects to Scene via buffer IDs.
- `Renderer` — Walks the scene graph and dispatches GL draw calls. Currently supports `triSolid@1` pipeline.

## Conventions

- C++17 required. Third-party C++ deps: RapidJSON (header-only, JSON parsing), GLAD (GL loader, vendored in `third_party/glad/`). System dep: `libosmesa6-dev` (headless GL for `dc_gl`).
- All TypeScript packages use ESM (`"type": "module"`).
- Workspace packages reference each other via `"workspace:*"` protocol.
- Library packages export directly from `./src/index.ts` (no build step; consumed by Vite).
- D-numbers (D1.1, D2.3, etc.) are informal milestone identifiers used in test names and comments.

## Workflow

- Use `feature/<name>` branches. Features can be stacked. Small changes can go directly on `main`.
- Tests are for regression defense, not coverage targets. Name test files after the feature milestone (e.g., `d1_1_smoke.cpp`).
