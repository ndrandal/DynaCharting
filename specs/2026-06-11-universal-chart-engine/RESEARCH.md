# DynaCharting: A GPU-Native Streaming Grammar of Graphics
### Can one engine render ANY chart from primitives + a JSON manifest fed by raw data?
*Decision-grade research synthesis — June 2026*

---

## 1. Executive Summary

**The verdict: YES for the data→pixels grammar — with a mandatory GPU-compute escape hatch — but NO for three orthogonal subsystems (interaction/selection state, true-3D rasterization, and stateful iterative layout as compositions) that the manifest can only *reference*, never *subsume*.** The honest universality figure is ~70% of the chart universe composable from a small fixed primitive basis, another ~15–20% reachable via named heavyweight primitives the engine ships and the manifest invokes, a residual spectral/field/topology tail reachable only through a custom-WGSL escape hatch, and a hard-walled remainder (true 3D, bidirectional interaction, per-instance picking) that lives in *separate architectural layers* no primitive can buy.

**The core idea in five sentences.** Today DynaCharting is a pure renderer: a JSON control plane builds a scene graph of draw items bound to 10 fixed-format GPU pipelines, and a binary data plane fills anonymous byte-buffers that must *already be* the exact vertex bytes a pipeline expects — every layout, binning, FFT, and tessellation is precomputed upstream. The proposed inversion inserts a thin, typed **data→visual layer** between data-ingest and render: named typed **columns** (a TABLE), **scales** (column domain → visual range, auto-domained, replacing the single baked affine `mat3`), **transforms** (filter/formula/bin/aggregate/stack/sort/window + a layout/spectral tail), and **encodings** (markChannel ← scale(column)) that compile down to the existing vertex formats *every frame* off a live feed. This is structurally the Vega/Vega-Lite grammar of graphics, but realized GPU-native, streaming/incremental, and AI-authored — running the transform DAG as per-frame WASM (and, for the parallel tail, WebGPU compute) passes, which no prior-art system does. The renderer stays dumb; the new layer is strictly additive and compiles to the same 10 pipelines. The walled tail (KDE, FFT/STFT, marching-squares, treemap/sankey/force layout) is handled either by named engine primitives or a typed, sandboxed custom-WGSL-compute-from-JSON node — the GPU-native analog of Vega's custom transforms, ECharts' `renderItem`, or ggplot2's custom Stat.

**The single most important conclusion.** The break in universality is *clean and diagnostic*: it is **not** that some transform is missing (the compute escape hatch can compute almost anything, including 3D meshes and graph embeddings). The break is that (a) the **render target is 2D, painter's-order, no-depth, no-camera**, so anything needing occlusion is unreachable from JSON; and (b) the manifest is a **pure one-directional `feed→frames` function**, so anything requiring mutable cross-frame state — interaction, selection, linked views, iterative layout, per-instance pick identity — sits in an orthogonal subsystem the data→visual layer cannot express. **Recommendation: build it, in phases, scoping interaction/3D as explicitly separate programs.** Phase 1 (table+scale+encode) alone kills the baked transform and renders the canonical financial views from raw data; Phase 2 (one new per-instance-color mark) collapses ~6 currently-walled views to native. Confidence: **High** on feasibility and on the boundary; the early phases are *generalization of an existing CPU recipe layer*, not greenfield.

---

## 2. The Thesis & The Inversion

### 2.1 The thesis

> Build a one-size-fits-all engine that renders any chart from (a) a fixed set of engine **PRIMITIVES** and (b) a JSON **MANIFEST**, fed by a stream of **RAW DATA** (the actual numbers — prices, measurements, events, relationships) — *not* precomputed geometry and *not* render instructions. A strong AI model authors the manifest declaring *how* raw data maps to primitives; the engine does the data→pixels lifting, live, every frame.

### 2.2 Why this inverts the showcase's premise

The current 22-view showcase proved breadth **only by precomputing everything upstream**. Concretely, in this very repo:
- `apps/showcase/views/treemap/` ships a 188–258-line squarified-layout + clip-space tessellation generator (`manifest.ts` / `records.gen.mjs`).
- `sankey` ships 208–232 lines of node-layout + ribbon tessellation (which the explainer concedes uses *straight* quads, not curves).
- `spectrogram` (279), `contour` (188), `density-heatmap` (2D KDE) all ship finished textures or geometry.

The engine receives **finished geometry/textures** and just draws them. The thesis **inverts the data boundary**: instead of shipping pixels-ready geometry, ship the *raw numbers* plus a declarative spec, and move the squarify/KDE/STFT/marching-squares math *into the engine*. The showcase is therefore the precise **negative space** of the vision — every walled view is a measured proof of a missing in-engine primitive.

### 2.3 Relation to prior art, and what is genuinely novel

The declarative grammar-of-graphics vocabulary has **converged** across Vega, Vega-Lite, ggplot2, D3, Observable Plot, ECharts, and Plotly onto a remarkably stable basis: ~7–9 marks `{point, line, area, rect, rule, arc, text, +image/geo}`, ~12–13 scale types, and a union of ~30 transforms. **Vega's transform list already includes `treemap`, `pack`, `partition`, `tree`, `contour`, `isocontour`, `kde2d`, `force`, `voronoi`, `pie`, `stack`** — i.e. most of what this showcase precomputed. That is strong positive evidence the "data→pixels in-engine" target is achievable for ~18 of 22 views.

What is **genuinely novel** (not copied from prior art):

| Dimension | Prior art | DynaCharting target |
|---|---|---|
| Execution substrate | CPU/JS dataflow (even Reactive Vega) | **WebGPU compute passes** running the transform DAG |
| Cadence | Static/batch (ggplot2, Plot, matplotlib) or CPU-reactive (Vega) | **Per-frame streaming/incremental** on a live append/updateRange feed |
| Authorship | Human-written specs | **AI-authored** manifest as constrained program synthesis against a typed grammar |
| Tail coverage | Plugins (d3-sankey, d3-hexbin) or escape to JS | Named GPU primitives + a typed, sandboxed **WGSL-from-JSON** escape hatch |

The reactive-dataflow-DAG model is *proven* (Reactive Vega, Satyanarayan et al.); running it as per-frame GPU compute on a streaming feed is the new ground. The append/updateRange byte-buffer model DynaCharting already has is a natural substrate for an incremental dataflow.

---

## 3. The Conceptual Model

The data→visual layer is a **typed, acyclic, streaming dataflow DAG** inserted at one clean seam: between data-ingest (`applyDataBatch → syncAllBuffers → store_.setCpuData`) and the render walk (`renderer_->render`) in `core/wasm/dc_engine_host.cpp` (~lines 206–220). This is the exact seam the existing CPU aggregators already exploit.

```
 RAW FEED  {t, streamKey, field, value}  (the ONLY substrate the manifest may assume)
     │   long→wide pivot (events sharing rowKey become one row)
     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  TABLE STORE        named typed columns  {name, dtype, bufferId}          │
│   f32 / i32 / cat(dict) / timestamp(i64 epoch-ms on CPU — no f64 on GPU)  │
│   filled tail-append by the existing 13-byte binary record (op 1/2)       │
└───────────────┬─────────────────────────────────────────────────────────┘
                │  column refs (NOT re-serialized)
                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  TRANSFORM DAG   pure nodes: ColumnRef[] → Column[]   (topo eval + dirty) │
│   CORE: filter formula bin aggregate stack sort window sample  (WASM/GPU) │
│   LAYOUT: treemap partition pack sankey dendrogram geoproject  (CPU)      │
│   SPATIAL: kde contour hexbin regrid  (GPU compute)                       │
│   SPECTRAL/CUSTOM: fft/stft + customCompute(WGSL)  (ESCAPE HATCH)         │
│   RELATIONAL: join/lookup  (edge-table → node-position table by key)      │
└───────────────┬─────────────────────────────────────────────────────────┘
                ▼
┌──────────────────────┐     ┌──────────────────────────────────────────┐
│  SCALES              │     │  COORDS                                   │
│  domain→range,       │ ──▶ │  cartesian | polar | geo{projection}      │
│  streaming auto-     │     │  (replaces the single baked affine mat3;  │
│  domain (running     │     │   mat3 demoted to pan/zoom viewport)      │
│  reducer, O(Δ))      │     └──────────────────────────────────────────┘
└──────────┬───────────┘
           ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  ENCODING   markChannel ← scale(field | expr | const)                    │
│   one mark INSTANCE per table row; each channel resolves to a number      │
│   (position/size) or RGBA8 (color), packed into the mark's vertex format  │
└───────────────┬─────────────────────────────────────────────────────────┘
                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│  ENCODE PASS  packs columns into the EXACT stride a pipeline requires     │
│   (validateDrawItem gate: format == requiredVertexFormat, else REJECT)    │
└───────────────┬─────────────────────────────────────────────────────────┘
                ▼
   10 EXISTING DAWN PIPELINES  → framebuffer   (renderer stays dumb)
```

**Streaming scheduler** runs alongside: class-1 nodes (running min/max, append marks, bin increment) every frame; class-2 (rolling window, STFT hop) on window/hop boundaries; class-3 (sort, treemap, force, global KDE) on a throttled/debounced cadence decoupled from the data tick; class-4 (percent-of-whole, global z-score) only with a manifest-declared baseline policy.

---

## 4. The Primitive Basis

### 4.1 Dataset / TABLE model

A table is a named control-plane resource holding equal-length named typed columns. A column **is** a buffer — just typed and named — so the existing binary feed fills it with no new wire format.

| Concept | Definition | Engine mapping |
|---|---|---|
| table | named set of equal-length columns + row count | new control resource over existing Buffer ids |
| column | `{name, dtype, bufferId}` | one ingest `CpuBuffer` reinterpreted by dtype |
| dtypes | `f32`, `i32`, `cat` (dict u32 + string palette), `timestamp` (i64 epoch-ms) | f32/i32 native; **timestamp stays CPU / normalized to relative f32 before GPU — WebGPU has no f64, and epoch-ms overflows the f32 mantissa (~16.7M): a real correctness trap** |
| row append | feed `op=APPEND` to a column buffer | unchanged; columns append in lockstep |
| updateRange | `op=2` patches a slice | unchanged |
| derived column | `{name, expr}` pure formula | CPU on append (DerivedBuffer precedent) v1; GPU formula later |

The named/typed column is *the* missing abstraction: scales need a domain, encodings reference columns by name, auto-domain needs a dtype.

### 4.2 SCALE primitives (~11) — replaces the baked affine

The single highest-value correctness change: replace `AutoScale::computeYRange` (a full O(N) rescan, `core/src/viewport/AutoScale.cpp`) with **streaming running-reducer auto-domain (O(Δ))**.

| Scale | Map | Auto-domain | Notes |
|---|---|---|---|
| linear | d→r | running min/max | default; subsumes affine sx/tx for data axes |
| log | positive→r | running min/max | finance log-price |
| pow / sqrt | xᵏ→linear | running min/max | area-true bubble size = sqrt |
| time | epoch-ms→r | running min/max (i64, f32 offset) | f64 trap; base epoch on CPU |
| band | category→slot+pad | growing ordered set | bar x, heatmap columns |
| point | category→position | growing ordered set | line-per-category, ticks |
| quantile | sample→k | streaming sketch (t-digest/P²) | quantile color bins |
| quantize | uniform cut→k | running min/max + k | binned color |
| threshold | breakpoints→k | static | choropleth steps |
| sequential color | numeric→ramp | running min/max | heatmap/weather-radar magnitude → RGBA8/row |
| diverging color | numeric→two-sided | running min/max + fixed mid | correlation −1..+1 |

Per-pixel color (KDE/spectrogram) uses a 256×1 LUT texture sampled in-shader, not per-row. Class-4 drifting domains (quantile, diverging-mid, percent-of-whole) **require** a manifest baseline policy (`fixedEpoch | decaying | referenceWindow`).

### 4.3 MARK primitives (8) — mapped to existing pipelines, new ones flagged

| Mark | Channels | Existing pipeline | NEW? |
|---|---|---|---|
| point | x,y,size,color,opacity,shape | `points@1` (uniform) | **YES** per-point color: `instancedPointColor` |
| rule | x,y,x2,y2,color,width | `line2d@1` / `lineAA@1` | per-rule color via per-instance color |
| rect | x,y,x2,y2,color,opacity,corner | `instancedRect@1` — **ONE uniform color** | **YES (keystone): `instancedRectColor` (Rect4 + RGBA8)** |
| line | x,y,color-of-series,width | `line2d@1` / `lineAA@1` | no (one color per series = one item) |
| area | x,y,y2,color,opacity | `triGradient@1` (per-vertex RGBA) | no (`Pos2Color4` carries per-vertex color) |
| arc | θ,θ2,r,r2,color | `triSolid@1`/`triGradient@1` + polar | **YES coordinate**: affine cannot do polar |
| text | x,y,text,size,color | `textSDF@1` (Glyph8) | **wire the atlas** (gap, not new pipeline) |
| image | x,y,x2,y2,texture | `texturedQuad@1` + setTexturePixels | no (escape hatch for KDE/spectrogram/contour-fill) |

**The keystone finding.** Every mark except candle and area collapses to **one uniform color per draw item** (`core/include/dc/scene/Types.hpp:77`) — which is *why* the showcase rasterizes weather-radar, correlation, and footprint to textures. The `Candle6` format (`x,open,high,low,close,halfWidth` + 2-way colorUp/colorDown, `Types.hpp:80-81`) is the **existing proof** the engine can do per-instance multi-channel encoding; it just hard-codes it to candles. Generalizing it into `instancedRectColor` (per-instance `x0,y0,x1,y1` + packed RGBA8 + optional scalar) is **the single highest-leverage addition** — it natively unlocks per-bar-color bars, heatmaps, weather-radar, correlation, treemap leaves, and the footprint price×time grid, **with zero compute**, because color and size are pre-resolved per row by the scale stage.

The complete delta vs. the existing 10 pipelines is exactly **three additions**: (1) `instancedRectColor`, (2) `instancedPointColor`, (3) a polar coordinate slot.

### 4.4 ENCODING / channel-binding model

`encoding = { channel → {scale, field|expr, value} }`. A mark instance is emitted per table row; each channel resolves `scale(row.field)` to a number or RGBA8, packed into the mark's target vertex format. `value` = constant, `expr` = derived formula, scale omitted = identity. Compilation: `(table, mark, encoding) → DrawItem + Geometry`, whose buffer the encode pass writes each frame (class-1 incremental, only appended rows).

---

## 5. The Transform / Compute Layer

### 5.1 Tiered transform catalog (where each runs, streaming class)

`N`=rows, `Δ`=appended this tick, `W`=window, `F`=FFT size.

| Transform | Tier | Target | Streaming class | Notes |
|---|---|---|---|---|
| **filter** | core | WASM-CPU (GPU compaction if N>~100k) | 1 incremental | predicate; GPU needs prefix-sum compaction |
| **formula** | core | WASM-CPU; GPU for N>~200k | 1 incremental | per-row expr; same AST → WASM loop or WGSL body |
| **bin** | core | WASM-CPU; **GPU atomic-u32** large-N | 1 incremental | increment target bin on append |
| **aggregate** (sum/mean/min/max/count/p50) | core | WASM-CPU; **GPU parallel reduction** | 1 (most), 2 (quantiles) | groupby+reduce |
| **stack** | core | WASM-CPU; **GPU decoupled-lookback scan** | 1 append-only, 3/4 normalize | wiggle/normalize → baseline policy |
| **sort/rank** | core | WASM-CPU | 3 global | maintain sorted/heap → O(log N)/insert |
| **window/rolling** | core | WASM-CPU (EMA O(1); rolling O(W)) | 1 (EMA), 2 (fixed-window) | EMA exists: `core/math/Ema.cpp` |
| **sample/lod (M4)** | core | WASM-CPU; **GPU** min-max reduction | 2 (viewport-hop) | LodManager hysteresis exists |
| **join/lookup** | relational | WASM-CPU (hash/index) | recompute on key change | **red-team gap: prerequisite for ALL edge-bearing charts** |
| **treemap (squarify)** | layout | WASM-CPU only | 3 (resquarify=resize-stable) | sequential recursion; not GPU-friendly |
| **partition / pack / dendrogram / stratify** | layout | WASM-CPU | 3 | recursive hierarchy walks |
| **sankey** | layout | WASM-CPU + CurveTessellator | 3 | node layout + ribbon routing |
| **geo-projection** | layout | WASM-CPU (per-vertex) or GPU | 1 per-vertex | needs pre-baked boundary polygons; non-affine |
| **force (N-body)** | layout | **GPU compute**, CPU fallback | **iterative/stateful** | engine-owned persistent sim buffers |
| **kde/density (2D)** | spatial | **GPU compute** (splat-accumulate) | 2/3 | I-KDE for streaming; prime GPU case |
| **contour/marching-squares** | spatial | **GPU compute** (per-cell) → line2d | 3 | variable-cardinality output |
| **hexbin** | spatial | WASM-CPU or GPU atomic | 1 | bin on hex lattice |
| **fft/stft** | spectral | **GPU compute** (Stockham) or WASM | 2 (per-hop) | **canonical escape-hatch case — no grammar has it** |

**Execution rationale (verified against the stack).** WASM-CPU is v1 for all core + all layout: single-threaded WASM is fine for class-1 O(Δ) and for inherently-sequential recursive layout (no parallelism to exploit). GPU compute is justified only where work is **large-N AND embarrassingly parallel** (bin/aggregate/stack/KDE/contour/FFT). Adding compute is **additive, not a platform port**: `DawnDevice` already holds a live `wgpu::Device`/`Queue` (`DawnDevice.hpp:246`), `waitUntil()` already implements the async map-pump readback under ASYNCIFY, and emdawnwebgpu fully exposes `createComputePipeline`/compute passes in-browser. The only prerequisites are adding `BufferUsage::Storage` (today all buffers are `Vertex|Index|CopyDst|CopySrc`, `DawnDevice.cpp:25`) and a `createComputePipeline`/`beginComputePass` path on `GpuDevice`.

**WebGPU hard limits to design around:** no f64 (f32-only); ≤256 invocations/workgroup; ≤16KB workgroup storage; ≤128 MiB storage-buffer binding; ≤8 storage buffers/stage; no dynamic allocation (variable-output → 2-pass count-then-write or worst-case alloc); float atomics absent (histogram in f32 needs CAS or integer reinterpret).

### 5.2 Dataflow & typing

A **Dataset** is an ordered set of named **Columns** `{name, dtype, length, bytes}`. A **Transform** is a pure node `inputs: ColumnRef[] → outputs: Column[]`, referencing upstream outputs by `id.field`, forming a DAG. Evaluation is topological; a per-node **dirty flag** (driven by the existing `ChartSession` touched-buffer set) gates recompute. Each transform declares an output schema as a function of input schema (`bin → {bin0,bin1,count}`; `aggregate → keys + measures`), so the compiler validates dtype compatibility **at manifest-load (fail fast)**, and that the terminal encode's packed stride equals the pipeline's `requiredVertexFormat`. Intermediate columns live in a typed `ColumnStore` (sibling of `CpuBufferStore`); only the final encoded geometry is uploaded via the existing dirty-range coalescing.

### 5.3 The escape hatch — custom WGSL / expression from JSON

**Two flavors.** *(a) Expression DSL* — a restricted, side-effect-free grammar (like Vega's: column refs, literals, `+ - * / %`, comparisons, `&& || !`, ternary, ~25 math/reducer functions; **no assignment, no loops, no user functions, no host access**). The same AST compiles to **either** a WASM-CPU tight loop **or** a generated WGSL body, so `filter`/`formula` get the GPU fast path for free. *(b) Custom WGSL compute kernel* — for the spectral/field/bespoke tail.

```json
{ "id": "stft", "transform": "customCompute",
  "wgsl": "@compute @workgroup_size(256) fn main(...) { ... }",
  "inputs":  [{ "binding": 0, "column": "audio.sample", "dtype": "f32", "access": "read" }],
  "outputs": [{ "binding": 1, "name": "mag", "dtype": "f32", "length": "frames*bins", "access": "write" }],
  "uniforms": { "fftSize": 1024, "hop": 256 },
  "dispatch": { "x": "frames", "y": "bins", "z": 1 },
  "recomputePolicy": { "onHop": 256 } }
```

**Sandboxing contract:** I/O *only* through declared storage-buffer bindings (no globals, no host calls); no dynamic alloc / recursion / unbounded loops (WGSL forbids these natively); static limits enforced at load (`workgroup_size≤256`, shared≤16KB, ≤8 storage, dispatch≤65535/dim, binding≤128MiB); f32/f16/i32/u32 only (epoch-ms pre-normalized to relative f32); output length a **declarable function of input lengths** (variable-cardinality topology → engine-owned max-bounded buffer + atomic count + compaction); WGSL **Tint-validated at pipeline creation** — a failure rejects the manifest node (no partial render), mirroring `frameFailed` semantics.

**What it buys:** exactly the four walls inexpressible in `{filter,bin,aggregate,stack,window,sort}` — FFT/STFT (no grammar provides it), 2D KDE field, marching-squares iso-extraction, bespoke per-cell field math. **What it canNOT buy** (decisive): it is a *stateless column→column COMPUTE* function inside the data path. It cannot capture an event, hold mutable cross-frame selection/navigation/camera state, route a pixel hit back to a row, coordinate two manifests, or rasterize 3D geometry with occlusion (it produces 3D *numbers* but feeds only the 2D, no-depth render path). This is the precise outer edge of the escape hatch.

---

## 6. The Manifest Format

### 6.1 Schema (eight sections) and the anti-nonsense typing contract

`data → transforms → scales → coords → marks(encodings) → facet → interaction → layers`, all nodes addressed by string id in **one namespace**. Four checks reject malformed specs *before any byte streams*:

1. **Reference resolution** — every `from`/`in`/`scale`/`lookup.from` resolves into `data.sources ∪ transforms ∪ scales`; dangling = compile error.
2. **Column-set inference** — each dataset's column set+dtypes is statically inferable (source columns; transform output = inputs − dropped + `as` outputs); a channel referencing a missing/wrong-dtype column fails.
3. **Channel↔scale↔dtype↔pipeline matrix** — a channel binds `scale(field)` only if the field dtype is accepted by the scale type, the channel is legal for the mark, the mark is legal for the pipeline, **and the resolved channel set covers the pipeline's required vertex/instance fields** (the GPU-specific clause mirroring `validateDrawItem`).
4. **DAG acyclicity + streaming-class coherence** — topo-sort must succeed; a `globalRecompute` node feeding a `perFrame` mark is downgraded + warned.

Net: the AI synthesizes **a program in a strongly-typed total DSL with no general iteration** (iteration confined to named transforms / the WGSL hatch) — the regime where program-synthesis-against-a-grammar is most tractable.

### 6.2 Worked manifest A — Candlestick + SMA(20) from raw OHLC events (all class-1 incremental)

```json
{
  "version": "dc-manifest/1", "id": "candles-sma",
  "data": { "sources": [{
    "id": "ohlc", "kind": "stream",
    "stream": {
      "match": { "streamKey": ["AAPL"], "field": ["open","high","low","close","volume"] },
      "rowKey": "t",
      "columns": {
        "t":{"from":"rowKey","dtype":"timestamp","role":"time"},
        "open":{"from":"field:open","dtype":"f32"}, "high":{"from":"field:high","dtype":"f32"},
        "low":{"from":"field:low","dtype":"f32"},  "close":{"from":"field:close","dtype":"f32"},
        "volume":{"from":"field:volume","dtype":"f32"} } },
    "retention": { "policy":"keepLast", "rows":5000 } }] },
  "transforms": [
    { "id":"withSma","in":"ohlc","op":"window",
      "window":{"field":"close","frame":[-19,0],"agg":"mean","as":"sma20","minPeriods":1},
      "stream":{"class":"incremental","cadence":"perFrame"} } ],
  "scales": [
    { "id":"xt","type":"time","domainFrom":{"data":"ohlc","field":"t"},"range":"width",
      "autodomain":{"mode":"data","stream":"extend"} },
    { "id":"yp","type":"linear","domainFrom":{"data":"ohlc","fields":["low","high"]},"range":"height",
      "nice":true,"autodomain":{"mode":"data","stream":"extend","pad":0.05} } ],
  "coords": { "type":"cartesian" },
  "marks": [
    { "id":"candles","type":"rect","from":"withSma","pipeline":"instancedCandle@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},
        "yOpen":{"scale":"yp","field":"open"},"yClose":{"scale":"yp","field":"close"},
        "yHigh":{"scale":"yp","field":"high"},"yLow":{"scale":"yp","field":"low"},
        "width":{"value":0.7,"unit":"bandFraction"},
        "color":{"condition":{"test":"close >= open","value":"#26a69a"},"value":"#ef5350"} } },
    { "id":"smaLine","type":"line","from":"withSma","pipeline":"lineAA@1",
      "encoding":{ "x":{"scale":"xt","field":"t"},"y":{"scale":"yp","field":"sma20"},
        "color":{"value":"#ffb300"},"strokeWidth":{"value":1.5} } } ],
  "layers": [{ "id":"main","region":{"x":[0,1],"y":[0,1]},"marks":["candles","smaLine"] }],
  "interaction": {
    "panZoom":{"x":{"scale":"xt","mode":"translateScale"},"y":{"scale":"yp","mode":"translateScale"}},
    "follow":{"scale":"xt","mode":"stickRight"} }
}
```
Per frame: pivot new events into `ohlc` (O(Δ)); extend the window mean for new rows only; update `xt`/`yp` running min/max; pack one `Candle6` instance + one `lineAA` vertex per visible row; upload dirty ranges. This is the 60–120 Hz happy path.

### 6.3 Worked manifest B — Treemap (replaces the 188-line precompute)

```json
{
  "version":"dc-manifest/1","id":"treemap-market",
  "data":{"sources":[{ "id":"bars","kind":"stream",
    "stream":{ "match":{"streamKey":["AAPL","MSFT","NVDA","TSLA"],"field":["open","close","volume"]},
      "rowKey":"t","groupKey":"streamKey",
      "columns":{ "symbol":{"from":"field:streamKey","dtype":"cat","role":"group"},
        "t":{"from":"rowKey","dtype":"timestamp"},"open":{"from":"field:open","dtype":"f32"},
        "close":{"from":"field:close","dtype":"f32"},"volume":{"from":"field:volume","dtype":"f32"} } },
    "retention":{"policy":"keepLast","rows":40,"per":"symbol"} }]},
  "transforms":[
    { "id":"byBucket","in":"bars","op":"bin","bin":{"field":"t","maxbins":5,"as":"bucket"},
      "stream":{"class":"incremental","cadence":"perFrame"} },
    { "id":"leaves","in":"byBucket","op":"aggregate",
      "aggregate":{"groupBy":["symbol","bucket"],
        "ops":[{"field":"volume","agg":"sum","as":"size"},
               {"field":"open","agg":"first","as":"o"},{"field":"close","agg":"last","as":"c"}]},
      "formula":[{"expr":"(c - o)/o","as":"perf"}],
      "stream":{"class":"windowed","cadence":"onHop"} },
    { "id":"tiles","in":"leaves","op":"treemap",
      "treemap":{"hierarchy":["symbol","bucket"],"size":"size","method":"squarify",
        "padding":0.006,"tile":"stable","as":{"x0":"x0","y0":"y0","x1":"x1","y1":"y1"}},
      "stream":{"class":"globalRecompute","cadence":{"debounceMs":100,"trigger":"onUpdate"}} } ],
  "scales":[
    { "id":"px","type":"linear","domain":[0,1],"range":"width" },
    { "id":"py","type":"linear","domain":[0,1],"range":"height" },
    { "id":"heat","type":"diverging","domainFrom":{"data":"tiles","field":"perf"},"domainMid":0,
      "range":["#ef5350","#3a3f4b","#26a69a"],"autodomain":{"mode":"symmetric","stream":"extend"} } ],
  "coords":{"type":"cartesian"},
  "marks":[
    { "id":"cells","type":"rect","from":"tiles","pipeline":"instancedRectColor@1",
      "encoding":{ "x":{"scale":"px","field":"x0"},"x2":{"scale":"px","field":"x1"},
        "y":{"scale":"py","field":"y0"},"y2":{"scale":"py","field":"y1"},
        "color":{"scale":"heat","field":"perf"} } },
    { "id":"labels","type":"text","from":"tiles","pipeline":"textSDF@1",
      "encoding":{ "x":{"scale":"px","field":"x0","offset":4},"y":{"scale":"py","field":"y0","offset":12},
        "text":{"field":"symbol"},"size":{"value":11} } } ],
  "layers":[{"id":"main","region":{"x":[0,1],"y":[0,1]},"marks":["cells","labels"]}],
  "interaction":{"panZoom":{"enabled":false}}
}
```
The 188-line hand-tessellation collapses to `bin → aggregate+formula → treemap` + a per-instance-color `rect` + `text`. The squarify math lives **in the engine** as the `treemap` op, scheduled on a 100 ms debounce (class-3), with `tile:"stable"` (resquarify) so a tile's color slot is continuous frame-to-frame.

### 6.4 Worked manifest C — Node-link force graph (the iterative/stateful tail)

This exercises every hard feature at once: a stateful `iterative` transform with engine-owned persistent positions, a relational **join/lookup** so edges resolve endpoints by `id`, a foreign-key `ref` typing constraint, drag-writeback, and the WGSL escape hatch for the inner physics.

```json
{
  "version":"dc-manifest/1","id":"force-network",
  "data":{"sources":[
    { "id":"nodes","kind":"stream","stream":{ "match":{"field:glob":"node.*.weight"},
        "rowKey":"field:capture(node.(?<id>[^.]+).weight, id)",
        "columns":{ "id":{"from":"rowKey","dtype":"cat","role":"key"},
          "weight":{"from":"field:value","dtype":"f32"} } } },
    { "id":"edges","kind":"stream","stream":{ "match":{"field:glob":"edge.*.weight"},
        "rowKey":"field:capture(edge.(?<src>[^-]+)-(?<dst>[^.]+).weight, src, dst)",
        "columns":{ "src":{"from":"capture:src","dtype":"cat","ref":{"data":"nodes","field":"id"}},
          "dst":{"from":"capture:dst","dtype":"cat","ref":{"data":"nodes","field":"id"}},
          "weight":{"from":"field:value","dtype":"f32"} } } } ]},
  "transforms":[
    { "id":"sim","in":"nodes","op":"force",
      "force":{ "nodes":"nodes","edges":"edges","nodeId":"id","edgeSrc":"src","edgeDst":"dst",
        "forces":[{"type":"manyBody","strength":-30},{"type":"link","distance":30,"strengthField":"weight"},
                  {"type":"center","x":0,"y":0},{"type":"collide","radiusField":"weight","radiusScale":"rNode"}],
        "alpha":{"start":1.0,"decay":0.02,"min":0.001,"reheatOnInsert":0.3},
        "as":{"x":"fx","y":"fy"} },
      "stream":{"class":"iterative","cadence":"perFrame","state":"persistent"},
      "compute":{ "backend":"wgsl",
        "passes":[{"entry":"integrate","dispatch":{"x":"ceil(nodeCount/64)"}}],
        "io":{"read":["nodes.weight","edges.src","edges.dst","edges.weight"],
              "readWrite":["sim.fx","sim.fy","sim.vx","sim.vy"]},
        "wgsl":"@group(0)@binding(0) var<storage,read_write> pos: array<vec2<f32>>; /* verlet integrate */ @compute @workgroup_size(64) fn integrate(){ /* repulsion+spring+center */ }" } } ],
  "scales":[
    { "id":"gx","type":"linear","domainFrom":{"data":"sim","field":"fx"},"range":"width",
      "autodomain":{"mode":"data","stream":"extend","pad":0.1} },
    { "id":"gy","type":"linear","domainFrom":{"data":"sim","field":"fy"},"range":"height",
      "autodomain":{"mode":"data","stream":"extend","pad":0.1} },
    { "id":"rNode","type":"sqrt","domainFrom":{"data":"nodes","field":"weight"},"range":[2,18] } ],
  "coords":{"type":"cartesian"},
  "marks":[
    { "id":"links","type":"rule","from":"edges","pipeline":"lineAA@1",
      "encoding":{
        "x":{"scale":"gx","field":"src.fx","lookup":{"on":"src","from":"sim","key":"id"}},
        "y":{"scale":"gy","field":"src.fy","lookup":{"on":"src","from":"sim","key":"id"}},
        "x2":{"scale":"gx","field":"dst.fx","lookup":{"on":"dst","from":"sim","key":"id"}},
        "y2":{"scale":"gy","field":"dst.fy","lookup":{"on":"dst","from":"sim","key":"id"}},
        "strokeWidth":{"field":"weight","scale":{"type":"linear","range":[0.5,3]}},
        "color":{"value":"#55607a","opacity":0.5} } },
    { "id":"verts","type":"point","from":"sim","pipeline":"instancedPointColor@1",
      "encoding":{ "x":{"scale":"gx","field":"fx"},"y":{"scale":"gy","field":"fy"},
        "size":{"scale":"rNode","field":"weight"},"color":{"value":"#8ab4f8"} } } ],
  "layers":[{"id":"main","region":{"x":[0,1],"y":[0,1]},"marks":["links","verts"]}],
  "interaction":{
    "panZoom":{"x":{"scale":"gx","mode":"translateScale"},"y":{"scale":"gy","mode":"translateScale"}},
    "drag":{"target":"verts","writes":["sim.fx","sim.fy"],"pin":true,"reheat":0.3} }
}
```
**Honest note:** the `force` node and the `drag` writeback are *not* pure-`feed→frames`. The engine owns the persistent simulation buffers and the writeback loop; the manifest only **names** the primitive and **declares** the binding. This is the boundary made concrete (§7).

### 6.5 AI-authorability

**Tractable** because the search space is a finite mark vocabulary (~8), finite scales (~11), and a finite-but-extensible transform vocabulary (~18 named + 1 hatch) — closer to "fill a Vega-Lite spec" (which current models do well) than open-ended codegen, and the prior-art convergence means the target vocabulary is stable and well-represented in pretraining. **Verifiable** because every check in §6.1 runs *without data*, giving the model localized errors ("scale `yp` type `log` rejects field `close` which can be ≤0") for an execution-guided repair loop. The model needs in context: a **grammar card** (mark/scale/transform vocab + pipeline→format table), a **feed schema descriptor** (available streamKeys/fields/dtypes; the f64-time-stays-CPU rule), the **streaming-class table**, and a few worked manifests. **Verification asset:** a manifest is a pure function `feed→frames`, so replay it against a corpus of synthetic feeds (empty, single-record, monotonic, all-equal/degenerate-domain, out-of-order, NaN, 10⁶-record burst) and assert invariants (finite channels, `x0≤x1`, positions in-pane, stride matches format, domain-contains-values, no NaN/Inf to a buffer) — the harness is the synthesis grading oracle. (Caveat: this purity holds only for the stateless tiers; force/drag/selection are outside it.)

---

## 7. Universality Analysis

### 7.1 Coverage across the chart universe (~90 types, 9 families)

Of an adversarial ~90-chart corpus, roughly **70%** reduce cleanly to `DATASET → SCALE → {filter,formula,bin,aggregate,stack,sort,window} → MARK channel` — all embarrassingly parallel, all mapping to existing pipelines. The hard tail clusters around four computational primitives the *basic* basis does **not** name, plus three orthogonal subsystems.

### 7.2 The four named-primitive walls (add a primitive — bounded)

1. **Iterative/fixed-point layout** — force graphs, t-SNE/UMAP, cartograms, streamlines: `state[N]=f(state[N-1])`. Engine-owned persistent simulation state + per-frame integrate; manifest *names* it. The hardest, most universal gap; **not fixable by a declarative primitive**, only by a named stateful one.
2. **Constrained recursive layout** — squarified treemap, sunburst, icicle, tidy-tree, sankey, Sugiyama: sequential, data-dependent recursion with global constraints. CPU named primitives, throttled.
3. **Topology extraction (variable cardinality)** — marching-squares, marching-cubes, Delaunay/Voronoi, hexbin, convex hull: output count is data-dependent. Engine-owned max-bounded buffer + atomic count + compaction.
4. **Geographic projection + ragged polygon ingest** — choropleth/cartogram need pre-baked boundary polygons (the *shapes* are reference geometry, not in the feed) + a nonlinear projection function. **Plus a deeper data-model wall** (below).

Secondary: polar/radial coordinates (closed-form, medium lift), FFT/STFT (named spectral op or hatch), `scan`/path-dependent emission (renko/point&figure).

### 7.3 The red-team results — the PRECISE residual walls even the hatch can't buy

Adversarial review of four hardest domains converged on a sharp boundary:

| Domain | What composes | What the escape hatch **cannot** buy |
|---|---|---|
| **Networks/hierarchy** | icicle, dendrogram, adjacency, arc, chord, sankey (stateless) — *after* adding `instancedRectColor`, polar, curve-tessellation, **a relational JOIN**, and wiring the glyph atlas | **Stateful iterative layout** (force/Sugiyama/edge-bundle/clustering) is engine-owned, not composed. **Interaction writeback** (drag-pin, expand/collapse, cross-view brush) is outside `feed→frames`. **JOIN is a concrete missing primitive** the briefs underweighted — without it no edge can be positioned from generic positions+edges. |
| **Geo-maps** | hexbin & square-bin maps (raw substrate = point rows) compose cleanly today + `instancedRectColor` | **The ragged-polygon/topology DATA MODEL is missing below the transform layer** — geo raw data is *itself geometry-shaped* (MultiPolygon→rings→variable-length coords; TopoJSON shared arcs), violating the flat-column premise. **Contiguous (diffusion) cartograms** = iterative PDE over shared topology — exceeds even the single-kernel hatch. **Cross-view linked selection** = session state. |
| **3D scientific fields** | parallel-coords, SPLOM, 2D quiver, 2D streamlines/LIC, 2D contour, precomputed-embedding scatter (with per-vertex-colored polyline + rotated-glyph marks) | **True-3D rasterization** (surface/volume/isosurface/3D-streamline): blocked *below* the manifest by 2D-only vertex formats, mat3-not-mat4, and **no depth buffer by explicit design** (`DawnDevice.cpp:608-614`). The hatch is a COMPUTE hatch (builds the mesh) not a RENDER hatch (cannot draw it occluded). **Linked brushing** + **in-engine t-SNE/UMAP** also walled. |
| **Bespoke/novel/interactive** | arbitrary STATIC novel encodings, data-anchored annotations, faceted small-multiples — genuinely covered | **An entire orthogonal LAYER:** (1) **signals/selections** — mutable runtime state from pointer/keyboard events that transforms read; (2) **per-instance pick identity** — verified: pick returns only a 24-bit *per-DrawItem* id (`DawnPickBackend.hpp:48-50`), so an instanced grid is one DrawItem and "which datum did the user touch?" is *physically unanswerable*; (3) **keyed object constancy + transition clock** — the `AnimationManager`/`Tween.hpp` is unwired and not data-bound. |

### 7.4 The four-tier universality boundary (the honest answer)

1. **Universal (composable from the basis)** — ~70%: TABLE + {linear/log/time/band/sequential/diverging} + {filter/formula/bin/aggregate/stack/sort/window} + {point/rect/line/area/rule/arc/text}. The convergent prior-art grammar, GPU-native and streaming.
2. **Universal via named heavyweight primitives (engine ships, manifest invokes)** — treemap/partition/pack/sankey/dendrogram, join/lookup, curve/arc tessellation, polar coords, geo-projection. Composable as "name-it-and-bind-it," **not derivable** from the basic algebra.
3. **Universal via the WGSL escape hatch (stateless compute)** — FFT/STFT (confirmed universal gap: *no* declarative grammar has it), 2D KDE, marching-squares, bespoke field math. The manifest is partly a typed shader-delivery vehicle — exactly as Vega custom transforms / ECharts `renderItem` / ggplot2 custom Stat, but GPU-targeted and sandboxed.
4. **Permanently OUTSIDE a pure `feed→frames` manifest** — *no primitive fixes these; they are different architectural layers:*
   - **Stateful/iterative layout** (force, Sugiyama, t-SNE/UMAP, contiguous cartograms): engine-owned named primitives with persistent state only.
   - **Bidirectional interaction / signals / selection / cross-view linking**: a second orthogonal `event→signal→predicate` dataflow that writes back into transform inputs.
   - **Per-instance pick identity**: a new renderer pick path (a small but real addition), prerequisite for any interactive selection.
   - **True 3D rasterization**: needs Pos3 formats + mat4 view/proj + a depth-stencil with `depthWriteEnabled` + 3D textures + a ray-march pass. A custom *render* shader (vs. the custom *compute* shader) is outside the hatch as designed.
   - **Ragged-polygon/topology data model** (geo): a missing dtype *below* the transform layer.

**How to scope/accept:** ship tiers 1–3 as "the grammar." Declare tier-4 interaction and 3D as **explicitly separate programs** the manifest *references but does not subsume*. Add `join`, `instancedRectColor`, polar coords, curve-tessellation, and a wired glyph atlas as bounded primitive additions (they unblock the entire stateless relational/hierarchical domain). Accept that contiguous cartograms and cross-view-linked interactivity are out of the data→visual layer's charter.

---

## 8. Validation Against the 22 Existing Views

Every showcase dataset is the same uniform record `{t, streamKey, field, value}`. Decomposing each from raw data (zero precompute):

| # | View | Transforms (from raw) | Scales | Marks | Verdict |
|---|------|----------------------|--------|-------|---------|
| 1 | candles | bin(OHLC) | lin x, lin y | candle | **native** |
| 2 | ohlc-bars | bin(OHLC) | lin x, lin y | candle(stick) | **native** |
| 3 | price-line-area | formula(baseline) | lin x, lin y | area | **native** |
| 4 | candle-overlays | bin, cumsum, window(SMA20) | lin x, 2× lin y | candle+rect+line | **native** |
| 5 | scatter | cumsum, zip-join | lin x, lin y | point | **native** |
| 6 | ecg | lag-1 segment | lin x, lin y | line | **native** |
| 7 | audio-waveform | aggregate(envelope), mirror | lin x/y, seq color | area | **native** |
| 8 | depth-ladder | pivot/reshape | band y, lin x | rect | **native** |
| 9 | volume-profile | bin(price)+aggregate(sum) | band y, lin x | rect | composed |
| 10 | renko | quantize-walk (stateful scan) | lin x/y, ord color | rect | composed (`scan`) |
| 11 | correlation-heatmap | log-return, window, Pearson agg | ord x/y, diverging color | **rect-grid** | **native after `instancedRectColor`** |
| 12 | ridgeline | histogram, 1D-KDE, offset-stack | lin x/y, ord+seq color | area | composed |
| 13 | streamgraph | stack(offset=wiggle) | lin x/y, ord color | area | composed (class-4 baseline) |
| 14 | sankey | aggregate, sankey-layout+ribbon | band y, lin width, ord color | ribbon+rect | composed (named primitive) |
| 15 | treemap | window-agg, squarify | layout-positional, diverging color | rect | composed (named primitive) |
| 16 | radial-seasonality | fold(cyclic bin), aggregate, polar | **polar coord**, seq color | arc+rule | composed (polar) |
| 17 | sports-shot-chart | direct + static court | lin x/y, ord color | point+line | **native** |
| 18 | footprint | bin(price×time), aggregate(sum) | band×band, lin len, seq color | **rect-grid+text** | **native after `instancedRectColor`** (the single best argument for per-instance channels) |
| 19 | density-heatmap | 2D Gaussian KDE, tone-map | seq color | field raster | **escape-hatch (compute)** |
| 20 | contour | upsample, quantize, marching-squares | seq color | iso-band+isolines | **escape-hatch (compute)** |
| 21 | spectrogram | Hann window, STFT/FFT, log-mag | lin x, log y, seq color | freq×time raster | **escape-hatch (compute)** |
| 22 | weather-radar | upsample, colormap | seq color | field raster | **native after `instancedRectColor`** (or texturedQuad) |

**Roll-up:** ~13/22 are **native** (small core transform set + linear/band/color scales + existing marks). +5 need named layout/coordinate primitives (treemap, sankey, polar, wiggle-stack, 1D-KDE) or `scan`. +3 (density-heatmap, contour, spectrogram) require the **compute escape hatch** for exact fidelity. Critically, **4 views (correlation, footprint, weather-radar, + the color half of contour/spectrogram) move from escape-hatch to native solely by adding one per-instance-color mark** — the cheapest, highest-leverage milestone, **with zero new compute**. The footprint view is the decisive proof that the foundational missing primitive is *per-instance channel binding*, not more compute.

---

## 9. What It Would Take

### 9.1 De-risking precedent (under-weighted)

A working CPU geometry-emit layer **already exists** and does "raw numbers → packed vertex format" today: `core/src/recipe/{Candle,Line,Area,Axis,Bollinger,Macd}Recipe.cpp`, `core/src/math/{Ema,Indicators,NiceTicks}.cpp`, `core/src/data/{CandleAggregator,DerivedBuffer,LodManager,TemporalFilter}.cpp`, `core/src/viewport/AutoScale.cpp`. These are hard-coded recipes; **the early phases generalize them into a manifest-addressable DAG, not greenfield.** Likewise, GPU compute is additive: Dawn already exposes it on the live device/queue with the `waitUntil()` readback pump in place.

### 9.2 Phased roadmap

| Phase | Goal | Key work / paths | Effort | Proof-demo |
|---|---|---|---|---|
| **0. Prereqs** | Unblock text + storage | Wire glyph atlas (`dc_engine_host.cpp:324`); add `BufferUsage::Storage` second path (`DawnDevice.cpp:25`) | **S** | Labeled mark renders; storage buffer round-trips |
| **1. Foundation: raw→scale→encode→mark** (kills baked transform) | Table + linear/time/band scales + point/line/rect/candle from raw `{t,field,value}`, no precompute | `TableStore`+pivot; scale engine w/ streaming auto-domain; encoding compiler (must satisfy `validateDrawItem`); demote mat3 to pan/zoom; reuse `IngestProcessor`,`NiceTicks`; replace `AutoScale` callers in `LiveIngestLoop.cpp` | **L (~5–7 pw)** | Candlestick + line **from raw OHLC** — reproduces the candle view via the manifest path |
| **2. Keystone mark: per-instance color/size + polar** | Per-cell color rect-grid, per-point color scatter, polar coords | New vertex format + `instancedRectColor@1` & `instancedPointColor@1`; seq/diverging color → RGBA8/row; polar slot in `Types.hpp`; generalizes the candle 2-color proof | **M (~3–4 pw)** | weather-radar + correlation + footprint + pie **collapse to native** |
| **3. CPU/WASM transform DAG + JOIN** | filter/formula/bin/aggregate/stack/sort/window/sample + relational join | Typed `ColumnStore`+topo eval+dirty flag from `ChartSession`; restricted expr DSL → WASM loop; streaming scheduler (class-1/2/3); **`Join.*`**; reuse `Ema`,`DerivedBuffer`,`CpuBufferStore` | **L (~6–8 pw)** | Histogram, streamgraph, SMA/RSI/MACD, volume-profile from raw; graph w/ pre-positioned nodes + join-resolved edges |
| **4. GPU compute fast path** | Large-N parallel transforms as compute passes | `GpuDevice` compute API; parallel reduction, decoupled-lookback prefix-sum, atomic-u32 histogram, 2D KDE splat; same AST→WGSL; reuse `waitUntil()` | **L (~6–8 pw)** | density-heatmap (KDE) + 1M-point M4 LOD at 60–120 Hz, no precompute |
| **5. Layout primitives (named)** | squarify treemap/partition/pack/sankey/dendrogram in-engine | CPU layout transforms; `stratify`; resquarify (stable streaming); expose `CurveTessellator` as ribbon/arc transform; throttled cadence | **L (~5–7 pw)** | treemap, sunburst, sankey, ridgeline, icicle from raw — replaces the upstream generators |
| **6. Escape hatch (custom WGSL)** | FFT/STFT, contour, bespoke field math from JSON-declared WGSL | `customCompute` node: declared WGSL + typed IO; Tint-validated; sandbox limits; max-bounded output + atomic count + compaction; Stockham FFT, marching-squares→line2d | **L (~6–8 pw)** | spectrogram (STFT), contour (marching-squares) from raw |
| **7. Manifest validator + AI harness** | Typed manifest checkable before any data flows; manifests testable as `feed→frames` | Validator (ref resolution, column-set inference, channel↔scale↔dtype↔format matrix, acyclicity, class coherence); property/golden/adversarial replay harness | **M–L (~4–6 pw)** | AI-authored manifest passes validator + survives adversarial feed corpus + pixel-matches reference |

**Total core path (0–7): ~35–50 person-weeks** for the stateless data→visual grammar + escape hatch.

### 9.3 Critical path & risks

**Critical path:** Phase 1 is the spine — every later phase depends on it; the riskiest item is the **encoding compiler satisfying `validateDrawItem`'s exact-stride contract** (get the byte-packing wrong and nothing renders). Phase 4 (storage buffers + compute API) gates Phase 6.

| Risk | Severity | Mitigation |
|---|---|---|
| **f32-only breaks time axes** (epoch-ms overflows f32 ~16.7M; no f64 in WGSL) | High | All time/domain math on CPU; GPU transforms consume pre-normalized relative-f32 time. Non-negotiable. |
| **Streaming recompute cost** (today `AutoScale`/`CandleAggregator`/`AggregationManager` are full O(N) on every append) | High | Phase 1 → O(Δ) running reducers; class-3 throttled, debounced off the data tick. |
| **Variable-cardinality output leaks the column contract** (contour/voronoi/chord/adaptive curves) | Medium | Engine-owned max-bounded buffers + atomic counter + compaction; manifest declares cap. |
| **ENC-558** (instanced backend caches GPU buffer per geometryId, ignores growth) | Medium | Backend dirty/version hook; must land before Phase 2 ships instanced marks at scale. |
| **Single-threaded WASM** caps CPU transform throughput | Medium | GPU compute (Phase 4) for large-N; CPU fine for class-1 O(Δ) + sequential layout. |
| **Hallucinated/malicious WGSL** | Medium | Tint validation at pipeline creation (reject node); storage-only IO; static limits; no host calls. |
| **AI emits malformed manifests** | Low-Med | Typed DAG + compiler-in-the-loop repair; few-shot anchors; feed-replay grading oracle. |

### 9.4 Explicitly deferred (separate programs, NOT the grammar)

- **Interaction/signals/selection/cross-view + per-instance pick** — an orthogonal `event→signal→predicate` subsystem + a per-instance pick-id renderer path. XL, architecturally distinct from `feed→frames`.
- **True 3D** — Pos3 + mat4 + depth-stencil (`depthWriteEnabled`) + 3D textures + ray-march. XL, separate render-architecture program.
- **Contiguous (diffusion) cartograms** — iterative PDE over shared topology; exceeds the single-kernel hatch.

---

## 10. Recommendation & Decision

### 10.1 Verdict (confidence-rated)

- **Is a one-size-fits-all *data→pixels* engine real? — YES. Confidence: High.** It cleanly covers the composed-and-walled middle the showcase proved only by precomputing upstream, and the build is strictly additive (compiles to the existing 10 pipelines; generalizes a shipping CPU recipe layer; Dawn already exposes compute).
- **Does universality require an escape hatch? — YES, mandatory. Confidence: High.** Every prior-art grammar (Vega, Vega-Lite, ggplot2, D3, ECharts, Plotly) hits a ceiling and ships one; the evidence is unanimous. FFT/STFT alone is a confirmed gap no declarative grammar provides.
- **Is "ANY chart" achievable from the data→visual layer alone? — NO. Confidence: High.** Iterative/stateful layout, bidirectional interaction/selection, per-instance picking, and true-3D rasterization are genuinely separate subsystems. The break is diagnostic: not a missing transform, but a 2D-no-depth render target and a one-directional `feed→frames` contract.

**The defensible claim:** *a GPU-native, streaming, AI-authored grammar of graphics that is universal for stateless 2D visualization, paired with (a) named primitives for the recursive/spectral tail and (b) explicitly-scoped orthogonal subsystems for interaction and 3D that the manifest references but does not subsume.*

### 10.2 Recommended first build (the minimal foundation)

Ship **Phases 0–2** as the proof-of-thesis MVP (~8–11 person-weeks):
1. **Phase 0** — wire the glyph atlas; add a `Storage` buffer path.
2. **Phase 1** — TableStore + scale engine (streaming auto-domain) + encoding compiler; **kill the baked transform**; render candlestick + line **from raw OHLC events** through the manifest path.
3. **Phase 2** — add `instancedRectColor` (the keystone) + `instancedPointColor` + polar coords.

This is the highest-leverage slice: Phase 1 *proves the inversion* on the canonical financial views, and Phase 2 *collapses ~6 walled views to native with zero compute*. It de-risks the entire program before committing to the GPU-compute and escape-hatch phases.

### 10.3 Open decisions for the founder

1. **Scope of "any chart."** Confirm that interaction/selection and true-3D are **out of charter** for the data→visual layer (and become separate programs). The grammar's universality claim is honest only with this boundary stated.
2. **Escape-hatch policy.** Accept author-supplied WGSL (powerful, a validation/security surface — mitigated by Tint + sandbox limits), or restrict to a **fixed library of pre-authored kernels** (safe, less universal)? Recommendation: ship the fixed library first, gate raw-WGSL behind the sandbox contract.
3. **JOIN as a Phase-3 must-have.** The red-team surfaced relational join/lookup as an underweighted prerequisite for *all* edge-bearing charts. Confirm it lands in Phase 3, not deferred.
4. **Per-instance pick identity timing.** Interactive selection is physically impossible without it (24-bit per-DrawItem id today). Decide whether to add the renderer pick path early (enables drill-down/tooltip/brush) or defer with interaction.
5. **Streaming baseline policies.** Confirm that class-4 transforms (percent-of-whole, global z-score, wiggle-stack) **must** declare a baseline policy in the manifest or be rejected — there is no well-defined streaming semantics otherwise.
6. **Target N and cadence per view.** The CPU-vs-GPU-compute split (Phase 3 vs 4) hinges on real data sizes; fixing target N (10⁴? 10⁶?) and frame budget per view class determines how much of Phase 4 is actually needed.