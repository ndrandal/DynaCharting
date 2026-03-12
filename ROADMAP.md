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

### Infrastructure
- **CI/CD Pipeline** — GitHub Actions for build + test (OSMesa headless GL in CI)
- **GLFW Live Window** — First interactive real-time demo from C++ (DC_HAS_GLFW)
- **Embedding Example** — Minimal C++ app or WebAssembly build demonstrating library integration

### Engine Refinements
- **Error Observability** — Richer error context from data ingestion (LiveIngestLoop, IngestProcessor)
- **Float Epsilon Consistency** — Centralized comparison utilities
- **Streaming Backpressure** — Flow control for high-frequency data sources

### Potential New Features
- **Multi-Window Support** — Multiple GL contexts rendering shared scene data
- **Custom Shader Hot-Reload** — Live shader editing for custom pipeline development
- **Scene Diffing / Snapshots** — Incremental scene updates over network
- **WebAssembly Target** — Emscripten build for browser embedding
