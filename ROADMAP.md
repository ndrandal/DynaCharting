# DynaCharting Feature Roadmap

All engine-level primitives from the original roadmap have been implemented (D1–D78). This document tracks what's next.

---

## Completed (D1–D78)

### Data Manipulation
- **Filtering** — Index buffer indirection (D26)
- **Sorting** — Order buffer for draw sequence control (D26)
- **Cross-Pane Data Binding** — SelectionState drives visibility across panes (D33)

### Interaction
- **Range / Box Selection** — Rectangular region selection (D33)
- **Drag & Drop** — Hit testing + drag state tracking (D35)
- **Cursor Hints** — Engine signals cursor intent based on hover state (D34)

### Visual
- **Animation / Transitions** — Tweening for transforms, colors, vertex data (D27, D48)
- **Gradient Fills** — Per-vertex and uniform-based color ramps (D28, D46)
- **Texture / Image Rendering** — texturedQuad@1 pipeline (D36, D41)
- **Dashed / Dotted Lines** — Per-draw-item dash pattern (D28)
- **Rounded Rectangles** — Corner radius on instancedRect (D28)
- **Clipping Masks** — Stencil-based masking (D29)
- **Blend Modes** — Per-draw-item: Normal, Additive, Multiply, Screen (D29)
- **Modern Styling** — 6 theme presets, post-process effects, pane borders, grid polish (D78)

### Data Flow
- **Event / Callback System** — EventBus with observer pattern (D42)
- **Computed / Derived Buffers** — Declarative buffer derivation (D32)

### Layout
- **Anchoring** — Position elements relative to pane edges (D37)
- **Virtualization** — VirtualRange for large datasets (D38)

### Accessibility
- **Keyboard Navigation** — Focus state + tab order (D39)
- **Semantic Annotations** — Role/label/value metadata for assistive tech (D40, D59)

### Performance
- **GPU Picking** — Color-ID render pass for pixel-perfect hit testing (D29)

---

## Next Up

### Renderer
- ✅ **WebGPU/Dawn cutover** (ENC-479…501) — `dc_gpu` (Dawn) is now THE renderer. The OpenGL backend (`dc_gl`) plus GLAD / OSMesa / GLFW were deleted (ENC-501). The headless render servers (`dc_showcase_server`, `dc_live_server`, `dc_dashboard_server`, `dc_gallery`) render via `DawnSceneRenderer` + `DawnDevice` readback. Rendering regression coverage is the Dawn render tests + Dawn-golden parity tests.
- ✅ **WASM + WebGPU browser package** (ENC-506) — `@repo/dc-wasm` compiles the C++ `dc` core to WebAssembly and renders via WebGPU, exposing an `EngineHost` TS surface. customer-layer was cut over to it (ENC-507).
- ✅ **TypeScript/WebGL2 prototype retired** (ENC-508) — `packages/engine-host`, `packages/chart-controller`, and `apps/demos/hello-engine` are **deleted**. The C++ core (`dc`/`dc_gpu`) owns rendering; `@repo/dc-wasm` is the browser/WASM path. The WebGPU/Dawn migration is complete.
- **On-screen Dawn surface** (ENC-497) — windowed/interactive presentation on Dawn. The old GLFW demos (`hello_glfw`, `d2_7`, `d5_7`, `d6_3`, …) were removed pending this; they needed an on-screen GL surface.

### Infrastructure
- **CI/CD Pipeline** — GitHub Actions for build + test (Dawn render tests need a Vulkan ICD; lavapipe fallback)
- **Embedding Example** — Minimal C++ app demonstrating library integration (the WebAssembly browser embedding is now `@repo/dc-wasm`, ENC-506)

### Reactive Bindings (D80, retroactively documented)
- **BindingEvaluator** — Reactive bindings between scene elements (selection / hover / viewport / data triggers → filter / range / visibility / color effects). See `docs/binding-evaluator.md` (ENC-88).

### Engine Refinements
- **Error Observability** — Richer error context from data ingestion (LiveIngestLoop, IngestProcessor)
- **Float Epsilon Consistency** — Centralized comparison utilities
- **Streaming Backpressure** — Flow control for high-frequency data sources

### Potential New Features
- **Multi-Window Support** — Multiple Dawn surfaces rendering shared scene data (after ENC-497)
- **Custom Shader Hot-Reload** — Live shader editing for custom pipeline development
- **Scene Diffing / Snapshots** — Incremental scene updates over network
- ✅ **WebAssembly Target** — Emscripten build for browser embedding, shipped as `@repo/dc-wasm` (ENC-506)
