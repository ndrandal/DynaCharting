# Building Chart Sessions

How to create a new chart demo from the single-file JSON document architecture.

## Quick Start

```bash
pnpm --filter @repo/demo-hello-engine dev   # http://localhost:5173/gallery.html
```

Two reference demos exist:
- **`gallery.html`** — 6 exotic charts in a 3x2 grid, every pipeline type, full theme system
- **`themes.html`** — simple candlestick+volume chart, 6 themes, good starting template

## Architecture: Single-File Driven

Each demo is one HTML page + one TypeScript file. The TypeScript file contains a **chart document** — a plain JSON object that declares the entire scene:

```
CHART_DOC (JSON)         Interpreter (TS code)
  ├─ meta                  ├─ buildScene()     → reads JSON, calls applyControl()
  ├─ scene.transforms      ├─ pushData()       → generates data, sends binary batches
  ├─ scene.buffers         ├─ applyTheme()     → maps roles → colors
  ├─ scene.geometries      └─ interaction      → pan/zoom/crosshair
  ├─ scene.drawItems
  └─ themes
```

The document defines **what** exists. The interpreter code defines **how** data is generated and interaction works.

## Step-by-Step: Creating a New Demo

### 1. Create HTML + TS entry

```
apps/demos/hello-engine/
  my-demo.html              ← page with <canvas id="c">
  src/main-my-demo.ts       ← imports EngineHost, defines chart document
```

Add to `vite.config.ts`:
```ts
input: {
  myDemo: new URL("./my-demo.html", import.meta.url).pathname,
}
```

### 2. Define the Chart Document

The document is a TypeScript object (type-checked, but plain JSON-serializable):

```ts
const CHART_DOC = {
  meta: { title: "My Chart", version: 1 },
  data: { /* parameters for your data generators */ },
  scene: {
    transforms: [
      { id: 1, tx: 0, ty: 0.5, sx: 1, sy: 0.45 },  // price panel
      { id: 2, tx: 0, ty: -0.5, sx: 1, sy: 0.45 },  // volume panel
      { id: 99, tx: 0, ty: 0, sx: 1, sy: 1 },        // overlay (identity)
    ],
    buffers: [
      { id: 100 },  // candle data
      { id: 200 },  // volume data
    ],
    geometries: [
      { id: 1000, kind: "instanced", bufferId: 100, format: "candle6", strideBytes: 24 },
      { id: 1100, kind: "instanced", bufferId: 200, format: "rect4",   strideBytes: 16 },
    ],
    drawItems: [
      { id: 5000, geoId: 1000, pipeline: "instancedCandle@1", transformId: 1, role: "candle" },
      { id: 5100, geoId: 1100, pipeline: "instancedRect@1",   transformId: 2, role: "volume" },
    ],
  },
  themes: { /* see Theme System below */ },
};
```

### 3. Build the Scene

```ts
const host = new EngineHost(hud);
host.init(canvas);
host.start();

for (const t of doc.scene.transforms) {
  host.applyControl({ cmd: "createTransform", id: t.id });
  host.applyControl({ cmd: "setTransform", id: t.id, tx: t.tx, ty: t.ty, sx: t.sx, sy: t.sy });
}
for (const b of doc.scene.buffers)
  host.applyControl({ cmd: "createBuffer", id: b.id });
// ... geometries, drawItems (see buildScene in gallery)
```

### 4. Push Data

All data is binary. Build a `Float32Array`, wrap it in the binary ingest format:

```ts
function sendF(bufferId: number, data: Float32Array) {
  const payload = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  const buf = new ArrayBuffer(13 + payload.byteLength);
  const u = new Uint8Array(buf);
  const dv = new DataView(buf);
  u[0] = 2;                                    // op: updateRange
  dv.setUint32(1, bufferId, true);              // buffer ID
  dv.setUint32(5, 0, true);                     // offset
  dv.setUint32(9, payload.byteLength, true);    // payload length
  u.set(payload, 13);
  host.enqueueData(buf);
}
```

### 5. Apply Theme

Map draw item roles to theme colors:

```ts
function applyTheme(theme: ThemeDef) {
  for (const di of doc.scene.drawItems) {
    if (di.role === "candle")
      host.applyControl({ cmd: "setDrawItemColor", id: di.id, colorUp: theme.up, colorDown: theme.down });
    if (di.role === "volume")
      host.applyControl({ cmd: "setDrawItemColor", id: di.id, color: theme.volume });
  }
  host.applyControl({ cmd: "setClearColor", r: theme.bg[0], g: theme.bg[1], b: theme.bg[2], a: theme.bg[3] });
}
```

---

## Available Pipelines & Formats

| Pipeline | Geometry Kind | Format | Stride | Data Layout |
|----------|-------------|--------|--------|-------------|
| `triSolid@1` | vertex | `pos2_clip` | 8 | `[x, y]` per vertex, 3 vertices per triangle |
| `line2d@1` | vertex | `pos2_clip` | 8 | `[x0, y0, x1, y1]` per line segment |
| `points@1` | vertex | `pos2_clip` | 8 | `[x, y]` per point |
| `instancedRect@1` | instanced | `rect4` | 16 | `[x0, y0, x1, y1]` per rectangle |
| `instancedCandle@1` | instanced | `candle6` | 24 | `[x, open, high, low, close, halfWidth]` |
| `textSDF@1` | instanced | `glyph8` | 32 | (requires GlyphAtlas setup) |

---

## Available Commands

| Command | Fields | Notes |
|---------|--------|-------|
| `createTransform` | `id` | |
| `setTransform` | `id, tx?, ty?, sx?, sy?` | Affine 2D: `[sx,0,0, 0,sy,0, tx,ty,1]` |
| `createBuffer` | `id` | Empty CPU buffer |
| `createGeometry` | `id, vertexBufferId, format, strideBytes` | Vertex geometry |
| `createInstancedGeometry` | `id, instanceBufferId, instanceFormat, instanceStrideBytes` | Instanced geometry |
| `createDrawItem` | `id, geometryId, pipeline` | |
| `attachTransform` | `targetId, transformId` | Links draw item to transform |
| `setDrawItemColor` | `id, color?, colorUp?, colorDown?` | RGBA arrays `[r,g,b,a]` |
| `setDrawItemClipRect` | `id, rect` | NDC scissor `[x0, y0, x1, y1]`, or omit to clear |
| `setClearColor` | `r, g, b, a` | WebGL clear color |
| `delete` | `kind, id` | kind: `drawItem`, `geometry`, `transform`, `buffer` |
| `setDebug` | `showBounds?, wireframe?` | Debug toggles |

---

## Non-Obvious Relationships & Gotchas

### Transform ↔ Coordinate Space

- Transforms map **data coordinates → NDC** (Normalized Device Coordinates, -1 to +1).
- The transform matrix is column-major mat3: `[sx, 0, 0,  0, sy, 0,  tx, ty, 1]`.
- `tx, ty` are in NDC. `sx, sy` scale data coordinates. A data point at `(0.5, 0.3)` with `sx=0.44, tx=-0.5` renders at NDC `x = 0.5 * 0.44 + (-0.5) = -0.28`.
- **Pan** changes `tx, ty`. **Zoom** changes `sx, sy`. Independent Y-axis zoom per panel = independent `sy` values sharing the same `sx`.

### Draw Order = Creation Order

- Draw items render in the order they're created (Map insertion order). There is **no z-index**.
- Create grids/backgrounds **first**, data **second**, overlays (crosshair, dividers) **last**.
- If you need to reorder, delete and recreate in the desired order.

### Buffer ↔ Geometry ↔ DrawItem Chain

```
Buffer (CPU bytes) → Geometry (interprets format) → DrawItem (renders with pipeline + transform)
```

- One buffer can back **multiple** geometries (e.g., same data rendered as both line2d and points).
- Geometry declares the format; the buffer is just raw bytes.
- Updating buffer data (via `enqueueData`) automatically triggers re-upload and re-render.
- Buffer IDs are arbitrary positive integers. Use a naming convention (100s for panel 1, 200s for panel 2, etc.).

### Binary Ingest Format

Every `enqueueData(arrayBuffer)` call sends one record:
```
[1 byte op] [4B bufferId LE] [4B offset LE] [4B payloadLen LE] [payload bytes]
```
- Op 1 = append (grows buffer). Op 2 = updateRange (overwrites at offset).
- For static charts, always use op 2 with offset 0 (full replace).
- For streaming charts, use op 1 to append and `bufferKeepLast` to cap size.
- Data is **not batched automatically** — each `enqueueData` call is one record. For multiple buffers, make multiple calls.

### Clip Rects (Scissor Clipping)

- Without clip rects, draw items render to the **entire canvas**. Transforms position things but don't clip.
- `setDrawItemClipRect` sets a **per-draw-item** scissor rectangle in NDC coords `[x0, y0, x1, y1]`.
- The engine converts NDC to pixel coordinates and applies `gl.scissor()` before each draw call.
- **Overlay items** (crosshair, dividers) should **not** have clip rects — they span the full canvas.
- Panel-specific items should all share the same clip rect matching their panel's NDC bounds.

### Theme Roles

- Draw items have a `role` string that the theme applicator uses to map colors. This is **not** an engine concept — it's a convention in the chart document.
- The engine only knows about `color`, `colorUp`, `colorDown`. The role → color mapping happens in your `applyTheme()` function.
- `colorUp`/`colorDown` only affect `instancedCandle@1`. All other pipelines use `color`.

### Data Normalization

- The engine renders whatever coordinates you provide, scaled by the transform. **You** are responsible for normalizing data to a consistent range.
- Convention: normalize data to `[-0.88, 0.88]` (constant `E` in gallery), leaving room for axis tick marks at the edges.
- If different data series have different Y ranges (e.g., price vs. volume), use **separate transforms** with different `sy` and `ty`.

### Multi-Panel Layout

To create a grid of panels:
1. Compute each panel's NDC center (`tx, ty`) and scale (`sx, sy`).
2. Create one transform per panel.
3. Assign draw items to their panel's transform.
4. Set clip rects to prevent visual overflow.
5. Draw divider lines on an identity transform (id 99) at panel boundaries.

Example for 2x2 grid:
```
Panel 0 (top-left):  tx=-0.5, ty=+0.5, sx=0.44, sy=0.44, clip=[-1, 0, 0, 1]
Panel 1 (top-right): tx=+0.5, ty=+0.5, sx=0.44, sy=0.44, clip=[ 0, 0, 1, 1]
Panel 2 (bot-left):  tx=-0.5, ty=-0.5, sx=0.44, sy=0.44, clip=[-1,-1, 0, 0]
Panel 3 (bot-right): tx=+0.5, ty=-0.5, sx=0.44, sy=0.44, clip=[ 0,-1, 1, 0]
```

### Interaction ↔ Transform

- Pan/zoom modifies the transform `tx, ty, sx, sy` and calls `setTransform`.
- To pan a specific panel, modify only that panel's transform.
- **Linked X-axis**: share `sx` and `tx` across panels, differ `sy` and `ty`.
- Mouse coordinates → NDC: `ndcX = (clientX / width) * 2 - 1`, `ndcY = 1 - (clientY / height) * 2`.
- NDC → data: `dataX = (ndcX - tx) / sx`.

### Performance

- The engine processes up to 4 data batches per frame (`MAX_BATCHES_PER_FRAME`). Excess batches queue.
- Each `enqueueData` call triggers a GPU buffer re-upload for touched buffers. Minimize calls for static data.
- For streaming data, use the worker pattern (see `ingest.worker.ts`) to generate data off the main thread.
- Draw calls = number of visible draw items. Fewer draw items = better. Combine data into fewer buffers when possible.

---

## File Structure After Cleanup

```
apps/demos/hello-engine/
  gallery.html              ← Multi-chart showcase (6 panels, all pipelines)
  themes.html               ← Single-chart theme switcher (simple template)
  src/
    main-gallery.ts         ← Gallery document + interpreters
    main-themes.ts          ← Themes document + interpreters
    protocol.ts             ← Worker stream protocol types
  ingest.worker.ts          ← Data streaming web worker
  vite.config.ts            ← Vite build config
  BUILDING_CHARTS.md        ← This file
```
