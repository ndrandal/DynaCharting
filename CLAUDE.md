# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DynaCharting is a high-performance real-time charting engine intended as an embeddable library for internal use. The long-term target is C++ doing the heavy lifting for both data processing and rendering. The TypeScript/WebGL2 frontend served as a prototype to prove out concepts (scene graph, pipelines, data ingestion) and will be progressively replaced as the C++ core gains rendering capability.

**Current milestone:** First render from the C++ side (D2.1 — achieved).

## Repository Layout

- **`core/`** — C++17 static libraries (`dc` and `dc_gl`). Scene graph, command processor, resource registry, pipeline catalog, and GL rendering backend. This is where most new work happens. Built with CMake.
  - `dc` — Pure C++ core (no GL deps). Scene graph, commands, pipelines.
  - `dc_gl` — OpenGL rendering backend. Links against `dc`, GLAD, and OSMesa. Contains `GlContext`, `OsMesaContext`, `ShaderProgram`, `GpuBufferManager`, `Renderer`.
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

CMake options: `DC_BUILD_TESTS` (default ON), `DC_WARNINGS_AS_ERRORS` (default OFF).

**Note:** Third-party deps (RapidJSON, GLAD) need to be present under `./third_party` (or path set via `-DTHIRD_PARTY_ROOT`). OSMesa (`libosmesa6-dev`) must be installed for `dc_gl` and the D2.1 render test. If OSMesa is not found, `dc_gl` is gracefully skipped.

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
