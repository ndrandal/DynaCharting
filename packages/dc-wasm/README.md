# @repo/dc-wasm

ENC-506 (P6.5) — the DynaCharting **WASM core + Dawn renderer**, packaged behind
an `EngineHost` TS API that **matches `@repo/engine-host`** so `customer-layer`
can swap to it (ENC-507) with near-zero change.

## What it is

A workspace package that wraps the Emscripten/emdawnwebgpu build of the C++ `dc`
core + ingest + the full Dawn renderer (`DawnSceneRenderer` + all backends),
exposing the same public surface as the WebGL2 prototype `@repo/engine-host`:

```ts
import { EngineHost } from "@repo/dc-wasm"; // same shape as @repo/engine-host

const host = new EngineHost();
host.init(canvas);          // loads the WASM module + binds the canvas
host.applyControl(scene);   // JSON command -> WASM CommandProcessor
host.applyDataBatch(bytes); // binary record batch -> WASM IngestProcessor
host.start();               // render loop: WASM renders offscreen, blits to canvas
const hit = host.pick(x, y);
const stats = host.getStats();
```

Re-exports the same types (`EngineStats`, `PickResult`, `TransformParams`,
`EngineHostHudSink`) and `PIPELINES` / pipeline types as `@repo/engine-host`.

## How the canvas is bound (WebGPU)

The WASM renderer renders into an **offscreen** RGBA8 target and reads the
framebuffer back (`DawnDevice::readFramebufferRGBA` — the pixel-validated path
from ENC-504). `EngineHost` blits those bytes onto the **caller-provided**
`<canvas>`'s 2D context via `putImageData`. The external canvas is owned by JS;
the WASM module never creates its own. (A native surface-from-canvas swapchain is
a later optimization.)

## Async-vs-sync divergence (documented stubs)

The engine-host API is synchronous; the WASM device acquisition + GPU readback
are async (ASYNCIFY). `EngineHost` keeps the **synchronous signatures** and
drives the async work internally:

- `init(canvas)` starts an async module load; commands/batches issued before the
  module is ready are buffered and replayed (`whenReady()` / `isReady()` expose
  readiness).
- `pick(x, y)` returns the **last known** result synchronously and refreshes
  asynchronously. Use `pickAsync(x, y)` for the fresh (awaited) result.
- `setDebugToggles` forwards a best-effort `setDebug` command; the visual debug
  state lives in the renderer (stub-safe — never throws).

## Build (artifacts are committed; rebuild with)

The `.js` + `.wasm` in `wasm/` are committed for direct consumption by Vite.
To rebuild from the C++ core:

```bash
source ~/emsdk/emsdk_env.sh
# RapidJSON is header-only + gitignored; provision once:
git clone --depth 1 https://github.com/Tencent/rapidjson.git third_party/rapidjson
pnpm -C packages/dc-wasm build:wasm   # == bash scripts/build-wasm.sh
```

The build target is `dc_engine_host` (EMSCRIPTEN-gated in `core/CMakeLists.txt`);
it is additive and **does not affect the native `dc` build**.

## Validate

- **Type-check:** `pnpm -C packages/dc-wasm build` (`tsc --noEmit`).
- **Node (core + ingest, no render):**
  `node packages/dc-wasm/scripts/validate-node.mjs` — exercises `applyControl`,
  `applyDataBatch`, buffer readback, and stats. Prints `PASS`.
- **Browser (render):** serve `examples/engine_host_demo.html` over http and open
  in a WebGPU Chrome:
  ```bash
  cd packages/dc-wasm/examples && python3 -m http.server 8080
  # http://localhost:8080/engine_host_demo.html
  ```
