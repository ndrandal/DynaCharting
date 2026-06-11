# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DynaCharting is a high-performance real-time charting engine intended as an embeddable library for internal use. The C++ core does the heavy lifting for both data processing and rendering. The TypeScript/WebGL2 frontend served as a prototype to prove out concepts (scene graph, pipelines, data ingestion); it has been **fully retired** (ENC-508) now that the C++ core owns rendering. The browser/WASM path is `@repo/dc-wasm` (C++ core compiled to WebAssembly, rendering via WebGPU).

**Current milestone:** WebGPU/Dawn is the C++ renderer (`dc_gpu`). The original OpenGL backend has been removed (ENC-501); the full pipeline set renders headless through Dawn with offscreen readback. The TypeScript/WebGL2 prototype (`engine-host`/`chart-controller`/`hello-engine`) has been retired (ENC-508) — `@repo/dc-wasm` is the browser path. Windowed/on-screen presentation is next (ENC-497).

## Repository Layout

- **`core/`** — C++17 static libraries (`dc` and `dc_gpu`). Scene graph, command processor, resource registry, pipeline catalog, and the WebGPU/Dawn rendering backend. This is where most new work happens. Built with CMake.
  - `dc` — Pure C++ core (no graphics-API deps). Scene graph, commands, pipelines, plus the backend-agnostic CPU-side buffer base `CpuBufferStore` (`dc/render/`).
  - `dc_gpu` — WebGPU/Dawn rendering backend — **THE renderer**. Built only with `-DDC_FETCH_DAWN=ON` (building Dawn is heavy); links `dc` and `dawn::webgpu_dawn`. Contains `DawnDevice`, `DawnSceneRenderer`, and the 10 per-pipeline Dawn backends. See the WebGPU/Dawn section under Build & Development Commands.
  - The OpenGL backend (`dc_gl`) and its deps (GLAD, OSMesa, GLFW) were **removed** in the WebGPU/Dawn migration (ENC-501). On-screen/windowed presentation on Dawn is a separate ticket (ENC-497).
- **`packages/dc-wasm/`** — WASM + WebGPU browser package (`@repo/dc-wasm`). **THE browser/WASM path.** Compiles the C++ `dc` core to WebAssembly and renders via WebGPU, exposing an `EngineHost` TS surface. This is what customer-layer consumes.
  - The original TypeScript/WebGL2 prototype (`@repo/engine-host`, `@repo/chart-controller`, and the `apps/demos/hello-engine` demo) is **RETIRED** (ENC-508). It proved out the scene graph, pipelines, glyph atlas, and data-ingestion concepts; those are now owned by the C++ core (`dc`/`dc_gpu`) and surfaced to the browser through `@repo/dc-wasm`.
- **`apps/live-viewer/`** — Standalone live-stream viewer (`@repo/live-viewer`). Independent of the renderer packages; talks to a headless render server.

## Build & Development Commands

### TypeScript (pnpm workspace)

```bash
pnpm install                                        # install all workspace deps
pnpm --filter @repo/dc-wasm build                   # type-check the WASM+WebGPU browser package
pnpm --filter @repo/live-viewer build               # build the live-stream viewer
```

### C++ Core (CMake)

```bash
cmake -B build -DTHIRD_PARTY_ROOT=./third_party     # configure (RapidJSON required)
cmake --build build                                  # build library + logic tests
ctest --test-dir build                               # run the logic tests
ctest --test-dir build -R dc_d1_1_smoke              # run a single test by name
```

The **default** build (no `-DDC_FETCH_DAWN`) builds `dc` + the pure-logic tests only — no renderer, fast, and needs no graphics API. To get the renderer + render/golden tests, opt into Dawn (see below).

CMake options: `DC_BUILD_TESTS` (default ON), `DC_WARNINGS_AS_ERRORS` (default OFF), `DC_FETCH_DAWN` (default OFF — see below).

**Note:** Only RapidJSON is needed under `./third_party` (or via `-DTHIRD_PARTY_ROOT`) for the default build. There are no longer any GLAD / OSMesa / GLFW dependencies — the GL backend was removed.

### WebGPU / Dawn backend (`dc_gpu`)

`dc_gpu` is the renderer, but Dawn is **OFF by default** because building it from source is heavy. The default `dc` build + logic tests are unaffected unless you opt in.

Enable Dawn (fetches and builds it from source via CMake `FetchContent`). Use Ninja:

```bash
cmake -B build-dawn -G Ninja -DTHIRD_PARTY_ROOT=./third_party -DDC_BUILD_TESTS=ON -DDC_FETCH_DAWN=ON
cmake --build build-dawn -j$(nproc)
# Dawn render tests need a real Vulkan ICD; on a headless box use the lavapipe fallback:
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json ctest --test-dir build-dawn -j$(nproc)
```

The Dawn build adds `dc_gpu`, the `dc_json_host` embedding host, the headless render servers (`dc_showcase_server`, `dc_live_server`, `dc_dashboard_server`, `dc_gallery`), the per-pipeline Dawn render tests, and the Dawn-golden parity tests.

- **Pinned Dawn revision:** commit `58263faefe3c52fac4656825c6d55f85ee3c7536` — the immutable tip of branch `chromium/7880` as of **2026-06-09**. We pin an explicit commit hash (never a moving branch) for reproducibility. Update this hash deliberately when bumping Dawn.
- **Source:** `https://dawn.googlesource.com/dawn`. Dawn's own dependencies are fetched with its `fetch_dawn_dependencies.py` helper (`DAWN_FETCH_DEPENDENCIES=ON`), so `depot_tools` is **not** required.
- **Build cost (heads-up):** build-from-source is **slow** — the first configure clones ~3-4 GB of Dawn + its third-party deps, and a full compile takes **30-60+ minutes** and needs `python3` and `ninja`. Subsequent incremental builds are fast. The full Dawn build is validated in CI (ENC-499).
- **Lean build:** Dawn samples, tests, benchmarks, fuzzers, node bindings, install rules, and Tint command-line tools/tests are all disabled. Dawn is built as a single monolithic static library.
- **Linked target:** `dc_gpu` links the Dawn monolithic WebGPU target `dawn::webgpu_dawn` (alias of `webgpu_dawn`) plus `dc`. When `DC_FETCH_DAWN=OFF` (or Dawn is unavailable), `DC_HAS_DAWN` is FALSE and `dc_gpu` (and everything that needs it — the host, the servers, the render tests) is gracefully skipped; the default `dc` + logic-test build is unaffected.
- `dc_gpu` is the full WebGPU/Dawn renderer: `DawnDevice` (offscreen target + readback), `DawnSceneRenderer` (the scene-walk mirror of the old GL `Renderer::render`), and the 10 per-pipeline backends (triSolid/triGradient/triAA/line2d/lineAA/points/instancedRect/instancedCandle/textSDF/texturedQuad) + picking.

#### Windowed (on-screen) presentation — `DC_DAWN_WINDOWED` (ENC-497)

The default Dawn build is **headless** (offscreen render + readback). On-screen presentation is an **additive, opt-in** build path enabled with `-DDC_DAWN_WINDOWED=ON` (default OFF, so the headless build — the 169 tests + the WASM/browser targets — stays lean and needs no windowing system).

```bash
cmake -B build-win -G Ninja -DTHIRD_PARTY_ROOT=./third_party \
  -DDC_FETCH_DAWN=ON -DDC_DAWN_WINDOWED=ON \
  -DFETCHCONTENT_SOURCE_DIR_DAWN=~/dawn-src
cmake --build build-win --target dc_gpu dc_dawn_window_demo
# Run on the display (force lavapipe if no HW adapter):
DISPLAY=:0 ./build-win/core/dc_dawn_window_demo --frames 30 --out /tmp/dawn_window.png
```

What `DC_DAWN_WINDOWED=ON` changes:
- Re-enables Dawn's **X11 Vulkan surface** (`DAWN_USE_X11=ON`). Wayland and Dawn's *bundled* GLFW stay OFF — we use the **system GLFW** (`find_package(glfw3)`, else the system lib + headers) and force the X11/XWayland backend at runtime (`glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11)`), matching how Chrome runs WebGPU here (`--ozone-platform=x11`).
- Compiles `core/src/gpu/DawnWindowContext.{hpp,cpp}` into `dc_gpu`, links the system GLFW, and defines `DC_DAWN_WINDOWED=1`.
- Builds `dc_dawn_window_demo` (the canonical embedding template that replaces the deleted GL `hello_glfw`).

- `DawnWindowContext` — owns a GLFW X11 window + a `wgpu::Surface`/swapchain bound to a `DawnDevice`. Surface is built from `glfwGetX11Display()`/`glfwGetX11Window()` via `wgpu::SurfaceSourceXlibWindow` (mirroring Dawn's own `webgpu_glfw` utils), configured `Fifo`/vsync at the window size. Each `presentFrame()`: renders the Scene through `DawnSceneRenderer` into the device's offscreen target (id 0, **unchanged** headless path), then runs a fullscreen-triangle **blit** pass sampling that target's color view into the swapchain texture (`surface.GetCurrentTexture()`), then `surface.Present()` + `glfwPollEvents()`. The blit (vs. baking pipelines per surface format) keeps the RGBA8Unorm scene pipelines untouched while the swapchain is its native BGRA8Unorm. `readSceneToPng()` (and the static `writeRgbaPng`) dumps the rendered target to a self-contained PNG (no zlib/stb dep) as a pixel proof of the windowed pipeline.

## Architecture

### Target Data Flow (C++ core)

The C++ core owns the scene graph and will own rendering. The intended flow is:
```
Data Source → C++ Processing → Scene Graph → C++ Rendering
```

### Browser Data Flow (`@repo/dc-wasm`, WASM + WebGPU)

The browser path runs the C++ `dc` core compiled to WebAssembly and renders through WebGPU. The retired TypeScript/WebGL2 prototype proved out this flow; `@repo/dc-wasm` now realizes it for real:
```
Worker (ingest)  →  ArrayBuffer batches  →  Main Thread Queue
  →  dc core (WASM, binary parsing)  →  GPU Buffer Sync  →  WebGPU Draw Calls
```

### Binary Ingestion Format (per record)

`[1B op] [4B bufferId (u32 LE)] [4B offsetBytes (u32 LE)] [4B payloadBytes (u32 LE)] [payload]`

Op codes: 1 = append, 2 = updateRange.

### Rendering Pipelines

Pipeline types (owned by the C++ core's `PipelineCatalog`; the retired TS prototype's `pipelines.ts` was the original reference):
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

- `CpuBufferStore` (`dc/render/`) — Backend-agnostic CPU-side buffer store: per-id CPU bytes, capacity, dirty-range coalescing, `UploadStats`. The Dawn upload path drives it through `GpuDevice` + a `BufferHandleResolver`. (This was the device-neutral half of the old GL `GpuBufferManager`.)

### C++ Dawn Renderer Classes (`dc_gpu`)

- `DawnDevice` — Headless WebGPU/Dawn device: offscreen render target + synchronous pixel readback (`readPixel`, top-down origin), buffers/textures/samplers, pipeline + bind-group creation, blend/clip permutations.
- `DawnSceneRenderer` — Walks the whole Scene and dispatches every DrawItem through the registered Dawn backends (clear, per-pane scissor, per-item frustum-cull + blend + clip + transform, pane borders/separators). The Dawn mirror of the old GL `Renderer::render`. Exposes `render(scene, store, W, H)` and `renderPick(...)`.
- Per-pipeline backends — one `IRendererBackend` per pipeline (`DawnTriSolidBackend`, `DawnLine2dBackend`, `DawnInstancedRectBackend`, `DawnTextSdfBackend`, `DawnTexturedQuadBackend`, …) plus `DawnPickBackend`, all reading CPU bytes from `CpuBufferStore`.

## Conventions

- C++17 required for `dc`; `dc_gpu` and anything including Dawn headers needs C++20 (Dawn's C++ wrapper). Third-party C++ deps: RapidJSON (header-only, JSON parsing). The renderer dep is Dawn, fetched from source when `-DDC_FETCH_DAWN=ON` (see WebGPU/Dawn section). No GLAD / OSMesa / GLFW.
- All TypeScript packages use ESM (`"type": "module"`).
- Workspace packages reference each other via `"workspace:*"` protocol.
- Library packages export directly from `./src/index.ts` (no build step; consumed by Vite).
- D-numbers (D1.1, D2.3, etc.) are informal milestone identifiers used in test names and comments.

## Workflow

- Use `feature/<name>` branches. Features can be stacked. Small changes can go directly on `main`.
- Tests are for regression defense, not coverage targets. Name test files after the feature milestone (e.g., `d1_1_smoke.cpp`).
