# SPEC: Migrate the DynaCharting renderer from OpenGL to WebGPU (Dawn)

**Slug:** webgpu-dawn-migration · **Date:** 2026-06-09 · **Status:** Draft
**Repo:** DynaCharting (primary) · also touches customer-layer
**Linear:** [WebGPU/Dawn renderer migration](https://linear.app/encultured/project/webgpudawn-renderer-migration-1faf4a694dda) · ENC-479 … ENC-508

## Problem

DynaCharting's C++ rendering backend (`dc_gl`) is built on OpenGL 3.3 core. This is a dead
end for the platform's stated cross-platform goals:

- **macOS is a first-class target**, but Apple deprecated OpenGL and froze it at 4.1; it is
  unmaintained and may be removed. The native engine has no future on Apple via GL.
- The platform maintains **two parallel renderers**: the C++ `dc_gl` backend and a
  TypeScript/WebGL2 prototype (`packages/engine-host`) that `customer-layer/apps/web`
  consumes today (`apps/web/package.json:16` → `link:../../../DynaCharting/packages/engine-host`).
  Shaders, pipeline specs, and ingest logic are implemented twice.

All GPU work is concentrated in `core/src/gl/Renderer.cpp` (~1,587 LOC, 10 pipelines,
243 raw GL call sites, inline GLSL). Context creation is cleanly abstracted behind
`core/include/dc/gl/GlContext.hpp` (OSMesa headless + GLFW windowed), but **draw dispatch is
not** — there is no device-level seam. A partial per-pipeline seam is drafted at
`core/include/dc/render/IRendererBackend.hpp` (ENC-90) but is GL-named (`initGL`) and lacks a
device abstraction.

## Proposed change

Migrate native rendering to **WebGPU via Dawn**, and unify the browser path by compiling the
C++ core to **WASM + WebGPU (Emscripten)** — retiring the TypeScript/WebGL2 prototype entirely.

End state:
- `dc_gpu` (Dawn/WebGPU) replaces `dc_gl`. WGSL shaders replace inline GLSL. The OpenGL
  backend, GLAD, and OSMesa are deleted.
- A device-agnostic `GpuDevice` abstraction sits beneath a finalized `IRendererBackend`
  per-pipeline seam. The same C++ renderer drives native (Dawn → Vulkan/Metal/D3D) and
  browser (Emscripten → `navigator.gpu`).
- `customer-layer/apps/web` consumes a new `@repo/dc-wasm` package (the WASM core) instead of
  `@repo/engine-host`. `packages/engine-host` (WebGL2) is deleted.
- Dawn is integrated via **CMake FetchContent (build-from-source)**.

**Approach — strangler pattern.** The 49 existing GL/render tests are the conformance suite
and the migration's pass/fail bar. We never have a broken renderer:
1. Add a `GpuDevice` device abstraction + finalize `IRendererBackend`; implement it over GL
   first (refactor only — all 49 GL tests stay green, GL stays default).
2. Stand up a headless Dawn device with synchronous pixel readback; get `triSolid@1` (D2.1
   first-render) passing end-to-end on Dawn.
3. Port pipeline-by-pipeline. Each pipeline ticket extracts the GL backend **and** adds the
   Dawn backend, validated by re-running that pipeline's existing GL tests against Dawn.
   Both backends compile until parity; GL is default until the cutover.
4. Tackle the cross-cutting hard cases (blend, stencil clipping, GPU picking, post-process,
   streaming upload). Flip default to Dawn, sweep all 49 tests, delete GL.
5. Compile the core to WASM/WebGPU, reach browser parity, cut `customer-layer` over, delete
   the TS prototype.

## Scope

- Native WebGPU/Dawn renderer reaching parity on all 49 GL/render tests.
- `GpuDevice` + finalized `IRendererBackend` abstraction; per-pipeline GL→Dawn migration of
  all 10 pipelines (`triSolid`, `line2d`, `points`, `triAA`, `triGradient`, `instancedRect`,
  `instancedCandle`, `lineAA`, `texturedQuad`, `textSDF`).
- Hard features: per-draw-item blend modes (D29.1), stencil clipping (D29.2), GPU color-ID
  picking (D29.3), post-process passes (D47/D78.2), streaming buffer upload
  (GpuBufferManager/IngestGpuSync).
- Headless Dawn (software adapter) + windowed Dawn surface; macOS (Metal) build & conformance.
- CI on headless Dawn (Linux + macOS matrix).
- WASM + WebGPU (Emscripten) browser build; new `@repo/dc-wasm` package; browser pipeline +
  ingest parity.
- `customer-layer/apps/web` cutover to `@repo/dc-wasm`; deletion of `dc_gl`, GLAD, OSMesa, and
  the TS/WebGL2 prototype (`packages/engine-host`, WebGL2 path of `chart-controller`,
  `apps/demos/hello-engine`).

## Non-goals

- New rendering features or new pipeline kinds — this is a backend migration, not a feature
  project. Visual output must match the GL baseline.
- Changing the scene-graph / command-processor / recipe / data-ingest **logic** (the 150
  backend-agnostic tests must not change behavior).
- Rewriting the wire/binary ingestion format or the embassy data-plane contract.
- D3D12/native-Windows productization (Dawn gives us D3D for free, but it is not a validated
  target here).

## Constraints

- **Conformance bar:** the 49 GL/render tests must pass on Dawn before GL is removed; the 150
  logic tests must stay green throughout (they are backend-agnostic).
- **No-broken-renderer invariant:** GL stays the default and compilable until the Dawn backend
  reaches full parity (strangler).
- **Real-time perf:** the streaming hot path (`writeRange`→coalesce→upload, currently
  `glBufferData`/`glBufferSubData` with `GL_DYNAMIC_DRAW`) must not regress on Dawn
  (`queue.writeBuffer`/staging). Sovereignty rules are unaffected (rendering only).
- **Sync readback:** tests rely on synchronous `readPixels()`; Dawn readback is async and must
  be wrapped synchronously for the headless test harness.
- C++17; CMake ≥3.20; CMake conditional-build model (`DC_HAS_*` graceful-skip) must be
  preserved for `DC_HAS_DAWN`.

## Affected systems

- **DynaCharting** — `core/` (`dc_gl` → `dc_gpu`, Renderer, GpuBufferManager, TextureManager,
  PostProcessPass, contexts), CMake, `third_party/` (remove GLAD), WASM build, `packages/`
  (delete engine-host, adapt chart-controller, add dc-wasm), `apps/demos`.
- **customer-layer** — `apps/web` dependency swap (`@repo/engine-host` → `@repo/dc-wasm`) and
  render call-site adaptation.
- **CI** — new headless-Dawn matrix (Linux software adapter + macOS Metal).

## Alternatives considered

- **wgpu-native (Rust) instead of Dawn.** Rejected: adds a Rust toolchain to a C++-first repo;
  Dawn is C++ and integrates via CMake FetchContent natively. (User decision: Dawn.)
- **Stay on OpenGL.** Rejected: dead on macOS; perpetuates the dual-renderer maintenance cost.
- **Two parallel `Renderer` implementations (no shared dispatcher).** Rejected: duplicates the
  ~600 LOC scene-walk/cull/scissor/blend/stencil logic. We instead share the dispatcher and
  abstract the device — aligning with the existing `IRendererBackend` direction.
- **Keep engine-host for the browser; native-only Dawn.** Rejected by user (full cutover): we
  want a single renderer. Browser parity comes via WASM+WebGPU.
- **Dawn prebuilt binaries / git submodule.** Rejected in favor of FetchContent build-from-
  source for reproducibility and per-target control (accepting slower first build / heavier CI).

## Risks

- **WGSL rewrites (no GLSL).** All inline shaders (~14 programs incl. pick variants) rewritten
  in WGSL. *Mitigation:* port pipeline-by-pipeline, gate each by its existing GL test.
- **Immutable PSO state model vs GL mutable global state.** Blend modes, line width, stencil
  state — set per-draw in GL — become baked pipeline permutations. *Mitigation:* a pipeline-
  permutation cache keyed on (pipeline, blendMode, stencil-state); dedicated D29.1 ticket.
- **Replacing the OSMesa headless harness.** No OSMesa equivalent in WebGPU. *Mitigation:*
  Dawn + a software Vulkan adapter (SwiftShader) for headless CI; a sync wrapper around async
  map for `readPixels()`. This is foundational and front-loaded (Phase 0).
- **Dawn build/toolchain weight in CI.** FetchContent build-from-source is slow. *Mitigation:*
  cache the Dawn build artifact in CI; pin a known-good Dawn revision.
- **Streaming hot-path regression.** Naive `writeBuffer` per range can be slower than GL.
  *Mitigation:* preserve dirty-range coalescing; staging-buffer strategy; perf sanity in the
  cutover sweep.
- **Browser/WASM scope + cross-repo cutover.** WASM+WebGPU and the customer-layer swap roughly
  double the project and add a second repo. *Mitigation:* native parity + delete must land
  before the WASM phase; customer-layer cutover is the final, isolated step behind a parity-
  proven `@repo/dc-wasm`.
- **WebGPU browser coverage.** Requires a WebGPU-capable browser (no WebGL2 fallback after
  retirement). *Mitigation:* confirm customer-layer's supported-browser matrix before deleting
  engine-host (open question).

## Acceptance criteria

- All 49 GL/render tests pass against the Dawn backend on Linux (headless) and macOS (Metal);
  all 150 logic tests remain green.
- `dc_gpu` is the default and only native backend; `dc_gl`, GLAD, and OSMesa are removed from
  the tree and CMake.
- Streaming-ingest tests (`d2_4_ingest`, `d81_3_range_upload`) pass with no perf regression vs
  the GL baseline on the streaming hot path.
- A WebGPU-capable browser renders all pipelines via the WASM core; ingest parity with the old
  engine-host path is demonstrated.
- `customer-layer/apps/web` renders charts via `@repo/dc-wasm` with no dependency on
  `@repo/engine-host`; `packages/engine-host` is deleted.
- CI runs the conformance suite on headless Dawn for Linux + macOS.

## Open questions

- **Supported-browser matrix for customer-layer.** Retiring engine-host removes the WebGL2
  fallback. Which browsers must the WASM/WebGPU path support, and is that acceptable to
  product? (Blocks the final deletion ticket.)
- **Dawn revision pinning + CI cache strategy.** Which Dawn commit do we pin, and where do we
  cache the build artifact (GitHub Actions cache vs prebuilt artifact store)?
- **chart-controller's fate.** Does `@repo/chart-controller` fold into `@repo/dc-wasm`, or
  remain a thin TS layer over the WASM core's API?
- **Software adapter choice for headless CI.** SwiftShader (Dawn-native) vs lavapipe — which
  is more reliable for pixel-exact conformance in CI?

## Tickets

Each ticket carries acceptance criteria + an in-flight validation block (start → build/test +
named conformance run → adversarial `/code-review`/verifier → comment evidence → Done + PR).

| Phase | Ticket | Depends |
|---|---|---|
| 0 Toolchain | ENC-479 Dawn FetchContent + DC_HAS_DAWN + dc_gpu | — |
| 0 | ENC-480 Headless Dawn device + sync readPixels | 479 |
| 1 Seam | ENC-481 GpuDevice + finalize IRendererBackend | — |
| 1 | ENC-482 GlDevice + route frame-level state | 481 |
| 1 | ENC-483 Backend registry + dispatcher rewire | 482 |
| 2 Pipelines | ENC-484 triSolid@1 first render (Dawn) | 480, 483 |
| 2 | ENC-485 Dawn buffer streaming | 484 |
| 2 | ENC-486 line2d + points | 484 |
| 2 | ENC-487 triAA + triGradient | 484 |
| 2 | ENC-488 instancedRect + rounded + indexed | 485 |
| 2 | ENC-489 instancedCandle | 485 |
| 2 | ENC-490 lineAA + dash | 485 |
| 2 | ENC-491 texturedQuad + TextureManager | 485 |
| 2 | ENC-492 textSDF + glyph atlas | 491 |
| 3 Hard | ENC-493 blend-mode pipeline permutations | 486–490 |
| 3 | ENC-494 stencil clipping masks | 484 |
| 3 | ENC-495 GPU color-ID picking | 488, 489, 491 |
| 3 | ENC-496 post-process passes | 491 |
| 4 Native | ENC-497 windowed Dawn surface | 484 |
| 4 | ENC-498 macOS/Metal build + conformance | 492–496 |
| 4 | ENC-499 CI headless Dawn matrix | 480, 498 |
| 5 Cutover | ENC-500 Dawn default + perf sweep | 492–496 |
| 5 | ENC-501 delete dc_gl/GLAD/OSMesa | 500 |
| 6 WASM | ENC-502 Emscripten WASM core + bindings | — |
| 6 | ENC-503 dc_gpu Emscripten WebGPU first canvas | 484, 502 |
| 6 | ENC-504 browser pipeline parity + demo | 503, 492 |
| 6 | ENC-505 WASM data-plane ingest | 503 |
| 6 | ENC-506 @repo/dc-wasm package | 504, 505 |
| 7 Retire | ENC-507 [customer-layer] swap to dc-wasm | 506 |
| 7 | ENC-508 delete engine-host + TS prototype | 507 |
