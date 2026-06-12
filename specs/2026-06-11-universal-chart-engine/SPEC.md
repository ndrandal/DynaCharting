# SPEC: DynaCharting — GPU-Native Streaming Grammar of Graphics

**Slug:** universal-chart-engine · **Date:** 2026-06-11 · **Status:** Draft
**Repo:** DynaCharting · **Linear:** [DynaCharting — GPU-Native Streaming Grammar of Graphics](https://linear.app/encultured/project/dynacharting-gpu-native-streaming-grammar-of-graphics-65ea2e0e86cf) (ENC-589 … ENC-621)

> **Design source of truth:** [`RESEARCH.md`](./RESEARCH.md) (decision-grade, codebase-grounded, 10 sections). This SPEC is the project-level summary; RESEARCH.md holds the full primitive catalogs, worked manifests, universality analysis, and the cited code paths.

## Problem
DynaCharting today is a **pure renderer**: a JSON control plane builds a scene graph of draw items bound to 10 fixed-format GPU pipelines, and a binary data plane fills anonymous byte-buffers that must *already be* the exact vertex bytes a pipeline expects. There is **no named-field data model, no scale primitive, and no data-transform/compute stage** — the only "transform" is a baked affine pan/zoom `mat3`. The 22-view showcase proved breadth *only by precomputing everything upstream* (treemap squarify, sankey tessellation, KDE, FFT, marching-squares) and shipping finished geometry/textures. That demonstrates the **negative space** of the goal: the engine cannot turn raw data into primitives itself.

## Proposed change
Insert a thin, typed **data→visual layer** between data-ingest and render, turning DynaCharting into a GPU-native, streaming, AI-authored **grammar of graphics**: a feed of **raw data** + a declarative **JSON manifest** + a fixed **primitive basis** → any (stateless, 2D) chart, computed in-engine, live. The layer = **TABLE** (named typed columns) → **TRANSFORMS** (filter/formula/bin/aggregate/stack/sort/window + a layout/spectral tail, run in WASM and/or WebGPU compute) → **SCALES** (column domain→visual range, streaming auto-domain, replacing the baked `mat3`) → **ENCODINGS** (markChannel ← scale(column)) → **MARKS** (mostly the existing pipelines + per-instance-color additions) → the existing 10 Dawn pipelines. The renderer stays dumb; the layer is strictly additive and generalizes the existing CPU recipe layer (`core/src/recipe/*`). A typed, sandboxed **custom-WGSL-compute-from-JSON escape hatch** covers the spectral/field tail (FFT/KDE/contour).

## Scope
- The stateless **data→visual grammar**: table, scales (~11), encodings, marks (8, incl. new per-instance-color rect/point + polar), and the transform DAG (core + layout + spatial + spectral).
- In-engine compute: WASM-CPU transforms + a WebGPU-compute fast path + the WGSL escape hatch.
- A typed **manifest format** + a load-time validator + a `feed→frames` replay/verification harness (the AI-authoring grading oracle).
- Validation against the existing 22 views (re-expressed from raw data).
- **Two forward-compatibility hooks baked into the foundation** (so interaction can be added later without a retrofit): (1) a *generic reactive input/dirty mechanism* that fires on any input changing — data now, signals later; (2) *stable per-row identity threaded through to each rendered instance* — the prerequisite for future per-instance picking.

## Non-goals
- **Bidirectional interaction / selection / linked views / signals** — an orthogonal `event→signal→predicate` subsystem. Out of charter; future separate program (foundation leaves the door open via the two hooks above).
- **Per-instance pick *path*** (the renderer change) — deferred with interaction; only the *identity threading* is in scope now.
- **True-3D rasterization** (surfaces/volumes/isosurfaces with occlusion) — needs Pos3 + mat4 + depth-stencil + 3D textures; separate render-architecture program.
- **Stateful iterative layout as pure manifest** (force graphs, t-SNE/UMAP, contiguous cartograms) — the engine may *own* these as named primitives later, but they are not pure `feed→frames`.

## Constraints
- **WebGPU f32-only / no f64:** epoch-ms time must stay on CPU and be normalized to relative f32 before GPU use (overflows the f32 mantissa ~16.7M otherwise). Non-negotiable.
- **Exact-stride contract:** the encode pass must satisfy `validateDrawItem` (packed stride == pipeline `requiredVertexFormat`) or nothing renders — the highest-risk item.
- **Streaming cost:** replace O(N) full rescans (`AutoScale`, `CandleAggregator`, `AggregationManager`) with O(Δ) running reducers; throttle/debounce global-recompute transforms off the data tick.
- **WebGPU compute limits:** ≤256 invocations/workgroup, ≤16KB workgroup storage, ≤8 storage buffers/stage, ≤128MiB binding, no dynamic alloc (variable-cardinality output → max-bounded buffer + atomic count + compaction), no float atomics.
- Additive only: must compile down to the existing 10 pipelines; the default render path stays unchanged.

## Affected systems
- **DynaCharting** (sole repo): `core/` (dc, dc_gpu) — new TableStore/ColumnStore, scale engine, encoding compiler, transform DAG, compute stage, new pipelines; `packages/dc-wasm` — EngineHost surface + manifest entry points; `apps/showcase` — re-express views from raw data as validation.
- No treaty/forum/embassy wire-contract changes (the feed format is the existing 13-byte record). No sovereignty impact (mock/showcase data).

## Alternatives considered
- **Keep computing upstream (status quo):** rejected — it's the negative space we're leaving; not data-aware, not reusable across feeds, can't react to a live feed.
- **Adopt Vega/Vega-Lite directly:** rejected — CPU/Canvas, batch/reactive, ~10–50k mark ceiling, human-authored; cannot do real-time GPU streaming at millions of marks. We borrow its *grammar*, not its engine.
- **Pure WGSL-codegen for everything (no fixed basis):** rejected as the primary path — unbounded validation/security/perf surface; used only as the sandboxed escape hatch for the tail.

## Risks
- **Encoding compiler vs. exact-stride contract** (High) → land the validateDrawItem-conformant encode pass early (Phase 1) with golden byte tests.
- **WebGPU compute through WASM/emdawnwebgpu unproven end-to-end** (High) → de-risk with a trivial compute spike in Phase 0 *before* committing to Phases 4/6.
- **Streaming recompute cost** (High) → O(Δ) reducers + class-based throttling.
- **Variable-cardinality output** (Med) → engine-owned max-bounded buffers + atomic count + compaction.
- **ENC-558 instanced-buffer growth caching** (Med) → must land before Phase-2 instanced marks ship at scale.
- **AI emits malformed/ hallucinated manifests/WGSL** (Med) → typed DAG + load-time validator + Tint validation + sandbox + replay-grading oracle + compiler-in-the-loop repair.

## Acceptance criteria
- A JSON manifest + a raw `{t,streamKey,field,value}` feed renders a **candlestick + SMA** view with **zero upstream precompute** (proves the inversion).
- Adding one per-instance-color mark **collapses weather-radar, correlation, and footprint to native** (no texture/geometry bake).
- ≥18 of the 22 existing views re-expressible from raw data (native or composed-via-named-primitive); density-heatmap/contour/spectrogram via the WGSL escape hatch.
- A WebGPU compute kernel runs end-to-end in the WASM/browser build (de-risk spike green).
- An AI-authored manifest passes the load-time validator and survives the adversarial feed corpus.

## Open questions
1. Scope confirmation: interaction/selection and true-3D **out of charter** (separate programs)? (Recommended: yes.)
2. Escape-hatch policy: author-supplied raw WGSL (sandboxed) vs. a fixed library of pre-authored kernels first? (Recommended: fixed library first, gate raw WGSL behind the sandbox.)
3. `join`/lookup confirmed as a **Phase-3 must-have** (red-team prerequisite for all edge-bearing charts)?
4. Per-instance pick *identity threading* in Phase 1 now; pick *path* deferred — agreed?
5. Target **N and frame budget** per view class (10⁴? 10⁶?) — determines how much of the GPU-compute phase is actually needed.

## Linear tickets

[Project board](https://linear.app/encultured/project/dynacharting-gpu-native-streaming-grammar-of-graphics-65ea2e0e86cf). Phases 0–2 are fine-grained ≤5h tickets (the MVP you'd start now); Phases 3–7 are detailed **epic** tickets to be split into ≤5h tickets when scheduled.

**Phase 0 — Prereqs & GPU-compute de-risk**
- ENC-589 [P0.1] Wire SDF glyph atlas into the WASM host (text marks)
- ENC-590 [P0.2] Add a Storage buffer-usage path to DawnDevice
- ENC-591 [P0.3] DE-RISK SPIKE: WebGPU compute pipeline end-to-end in WASM

**Phase 1 — Foundation: raw → scale → encode → mark (kills the baked transform)**
- ENC-592 [P1.1] TableStore + Column model
- ENC-593 [P1.2] Long→wide pivot ingest
- ENC-594 [P1.3] HOOK: stable per-row identity threaded to instances
- ENC-595 [P1.4] HOOK: generic reactive dirty/recompute mechanism
- ENC-596 [P1.5] Linear scale + streaming O(Δ) auto-domain
- ENC-597 [P1.6] Time scale (f64-trap mitigation)
- ENC-598 [P1.7] Band + point ordinal scales
- ENC-599 [P1.8] NiceTicks integration
- ENC-600 [P1.9] Encoding / channel-binding model
- ENC-601 [P1.10] Encode pass + validateDrawItem exact-stride contract (RISK)
- ENC-602 [P1.11] Demote baked affine mat3 to pan/zoom
- ENC-603 [P1.12] Wire point + line marks
- ENC-604 [P1.13] Wire rect + candle marks
- ENC-605 [P1.14] Manifest v0 parser
- ENC-606 [P1.15] PROOF: candlestick + SMA(20) from raw OHLC via manifest
- ENC-607 [P1.16] Replace AutoScale callers in live ingest

**Phase 2 — Keystone mark: per-instance color/size + polar**
- ENC-608 [P2.1] instancedRectColor pipeline (the keystone)
- ENC-609 [P2.2] instancedPointColor pipeline
- ENC-610 [P2.3] Sequential color scale → RGBA8 per row
- ENC-611 [P2.4] Diverging color scale + class-4 baseline policy
- ENC-612 [P2.5] 256×1 color-LUT texture path
- ENC-613 [P2.6] Polar coordinate slot + arc mark
- ENC-614 [P2.7] Instanced-backend buffer-growth/version hook (ENC-558)
- ENC-615 [P2.8] PROOF: weather-radar + correlation + footprint go native

**Phases 3–7 — epics (decompose into ≤5h tickets when scheduled)**
- ENC-616 [P3] CPU/WASM transform DAG + core transforms + JOIN
- ENC-617 [P4] GPU compute fast path (gated by the P0.3 spike)
- ENC-618 [P5] Layout primitives (named, in-engine)
- ENC-619 [P6] Escape hatch: custom WGSL compute from JSON (gated by P0.3)
- ENC-620 [P7] Manifest validator + AI-authoring verification harness

**Future (deferred)**
- ENC-621 [FUTURE] Interaction layer design appendix
