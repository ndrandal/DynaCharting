# DynaCharting — Authoring Guide

This document teaches you how to build **anything** with DynaCharting's JSON interface. It is not a catalog of chart types. It is a guide to thinking in composable primitives so you can construct any 2D visualization — charts, dashboards, diagrams, heatmaps, custom UI — from a small set of building blocks.

---

## 1. Mental Model

DynaCharting is a **drawing engine**, not a charting library. It knows nothing about prices, timestamps, indicators, or axes. It knows:

- **Panes** — rectangular viewport regions with scissored rendering
- **Layers** — ordered containers inside panes
- **DrawItems** — things that get drawn: triangles, lines, points, rectangles, text
- **Buffers** — raw bytes that hold vertex data
- **Geometries** — descriptions of how to interpret buffer bytes
- **Transforms** — 2D affine mappings applied to vertex positions

There is no `createChart` command. You build charts the same way you build anything else: by placing geometric primitives in coordinate space, coloring them, and layering them.

The key insight: **every visual is a composition of positioned, styled geometry**.

---

## 2. Two Interfaces

**Imperative commands** — JSON strings applied one at a time via `applyJsonText()`. Good for incremental updates, streaming data, surgical modifications.

```json
{"cmd":"createPane","id":1,"name":"Main"}
{"cmd":"setPaneRegion","id":1,"clipYMin":-0.95,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":0.95}
```

**Declarative documents** — A single JSON object describing the entire scene. Fed to `SceneReconciler`, which diffs it against the live scene and emits the minimal set of imperative commands.

```json
{
  "version": 1,
  "panes": { "1": { "name": "Main", "region": { "clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95 } } }
}
```

Both produce the same scene. Use declarative for full-scene snapshots, imperative for live updates. They compose freely — reconcile a document, then stream commands on top.

---

## 3. Coordinate Spaces

Three spaces, one pipeline:

### Pixel Space
Origin top-left, Y points down, units are screen pixels. This is where the user clicks and where viewport dimensions live. You never write vertex data in pixel space directly.

### Clip Space
Origin center, Y points up, range [-1, 1] on both axes. This is what the GPU sees. Pane regions are defined in clip space. Vertex data after transform application must land in clip space to be visible.

```
(-1,+1) ──────── (+1,+1)
   │                  │
   │    (0,0)         │
   │                  │
(-1,-1) ──────── (+1,-1)
```

### Data Space
Your application's coordinate system — timestamps, prices, indices, percentages, whatever. Vertex data is written in data space. A **Transform** maps it to clip space.

**The flow:** Data Space → Transform → Clip Space → Scissor (Pane Region) → Pixels

---

## 4. The Six Primitives

Every entity in the scene is one of six types. They form a strict hierarchy:

```
Pane
 └── Layer
      └── DrawItem ← binds to → Geometry ← reads from → Buffer
                   ← styled by → Transform (optional)
```

### Buffer
Raw byte storage. Holds vertex data. Has no opinion about format — it's just bytes.

```json
{"cmd":"createBuffer","id":100,"byteLength":0}
```

`byteLength:0` means "empty, will be filled later." Data arrives via binary ingest (append or update-range).

### Geometry
Tells the engine how to interpret buffer bytes: which format, how many vertices.

```json
{"cmd":"createGeometry","id":101,"vertexBufferId":100,"format":"rect4","vertexCount":20}
```

Format is **immutable** — to change it, delete and recreate the geometry. Vertex count can be updated anytime:

```json
{"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":40}
```

### Transform
A 2D affine mapping: `clipPos = dataPos * scale + translate`.

```json
{"cmd":"createTransform","id":50}
{"cmd":"setTransform","id":50,"sx":0.01,"sy":2.0,"tx":-0.5,"ty":0.0}
```

The mat3 is column-major: `[sx, 0, 0,  0, sy, 0,  tx, ty, 1]`. Fields merge — omitted fields keep their current values. Default is identity (sx=1, sy=1, tx=0, ty=0).

**Shared transforms** are powerful: attach the same transform to candles, overlay lines, grid, and annotations. When the user pans/zooms, update one transform and everything moves together.

### Pane
A rectangular region of the viewport. Clips all rendering inside it via GL scissor.

```json
{"cmd":"createPane","id":1,"name":"Price"}
{"cmd":"setPaneRegion","id":1,"clipYMin":0.1,"clipYMax":0.95,"clipXMin":-0.95,"clipXMax":0.85}
```

Region fields are in clip space. A pane at `clipYMin:-0.95, clipYMax:0.95, clipXMin:-0.95, clipXMax:0.95` fills nearly the full viewport (leaving a thin margin). Multiple panes stack vertically or tile the viewport however you want — they're just rectangles.

Optional clear color fills the pane before drawing:

```json
{"cmd":"setPaneClearColor","id":1,"r":0.05,"g":0.05,"b":0.08,"a":1}
```

### Layer
An ordered container of DrawItems within a pane. Layers control draw order within a pane — lower-ID layers render first (behind), higher-ID layers render on top.

```json
{"cmd":"createLayer","id":10,"paneId":1,"name":"Grid"}
{"cmd":"createLayer","id":20,"paneId":1,"name":"Data"}
{"cmd":"createLayer","id":30,"paneId":1,"name":"Overlays"}
```

Layer 10 draws behind layer 20, which draws behind layer 30.

### DrawItem
The actual visible thing. A DrawItem has: a layer (where it lives), a pipeline (how to draw), a geometry (what to draw), optionally a transform and styling.

```json
{"cmd":"createDrawItem","id":200,"layerId":20,"name":"PriceLine"}
{"cmd":"bindDrawItem","drawItemId":200,"pipeline":"lineAA@1","geometryId":101}
{"cmd":"attachTransform","drawItemId":200,"transformId":50}
{"cmd":"setDrawItemStyle","drawItemId":200,"r":0.3,"g":0.6,"b":1.0,"a":1.0,"lineWidth":2.0}
```

---

## 5. Pipelines — What Can Be Drawn

Each pipeline defines a vertex format, a draw mode, and a shader. Choosing a pipeline means choosing what shape your bytes become.

### Filled Shapes

| Pipeline | Format | Stride | What It Does |
|----------|--------|--------|-------------|
| `triSolid@1` | pos2_clip (8B) | `[x, y]` × 3 per tri | Solid single-color triangles |
| `triAA@1` | pos2_alpha (12B) | `[x, y, alpha]` × 3 | AA triangles with edge fringe |
| `triGradient@1` | pos2_color4 (24B) | `[x, y, r, g, b, a]` × 3 | Per-vertex colored triangles |

Any polygon can be triangulated into `triSolid@1`. Use `triAA@1` for smooth edges (set alpha=1 at interior vertices, alpha=0 at edge vertices). Use `triGradient@1` when each vertex needs its own color.

### Lines

| Pipeline | Format | Stride | What It Does |
|----------|--------|--------|-------------|
| `line2d@1` | pos2_clip (8B) | `[x, y]` × 2 per segment | 1px lines, no AA |
| `lineAA@1` | rect4 (16B) | `[x0, y0, x1, y1]` per segment | AA lines with width, optional dash |

`lineAA@1` is the workhorse — it expands each segment into a quad with configurable `lineWidth`, `dashLength`, `gapLength`.

### Points

| Pipeline | Format | Stride | What It Does |
|----------|--------|--------|-------------|
| `points@1` | pos2_clip (8B) | `[x, y]` per point | Point sprites with `pointSize` |

### Instanced Shapes

| Pipeline | Format | Stride | What It Does |
|----------|--------|--------|-------------|
| `instancedRect@1` | rect4 (16B) | `[x0, y0, x1, y1]` per rect | Axis-aligned rectangles |
| `instancedCandle@1` | candle6 (24B) | `[x, open, high, low, close, hw]` | OHLC candlesticks |
| `textSDF@1` | glyph8 (32B) | `[x0,y0,x1,y1,u0,v0,u1,v1]` | SDF text glyphs |
| `texturedQuad@1` | pos2_uv4 (16B) | `[x, y, u, v]` | Textured rectangles |

`instancedRect@1` can represent bars, filled regions, backgrounds, UI panels, progress bars — anything rectangular. It supports `cornerRadius` for rounded corners.

`instancedCandle@1` auto-colors based on `close >= open` → `colorUp` else `colorDown`.

---

## 6. Draw Order

Draw order is **deterministic and ID-driven**. The renderer walks:

```
for each pane (ascending ID):
  set scissor to pane region
  clear pane (if clear color set)
  for each layer (ascending ID, filtered to this pane):
    for each drawItem (ascending ID, filtered to this layer):
      draw
```

**Lower IDs draw first (behind). Higher IDs draw on top.** This is the only rule.

Plan your ID scheme accordingly. A common pattern:

| ID Range | Purpose |
|----------|---------|
| 1–9 | Panes |
| 10–99 | Layers (grid: 10, data: 20, overlays: 30, labels: 40) |
| 100–199 | Buffers |
| 200–299 | Geometries |
| 300–399 | DrawItems (grid lines, then data, then overlays) |
| 50–59 | Transforms |

The exact numbers don't matter. What matters is that your grid DrawItem has a lower ID than your data DrawItem, which has a lower ID than your overlay DrawItem — within the same layer. Or better: put them on separate layers where the layer IDs encode the order.

---

## 7. Spatial Reasoning — Fitting Data Into View

This is the core skill. Given data in some range, how do you make it visible?

### The Transform Formula

```
clipX = dataX * sx + tx
clipY = dataY * sy + ty
```

To map data range `[dataMin, dataMax]` into clip range `[clipMin, clipMax]`:

```
scale = (clipMax - clipMin) / (dataMax - dataMin)
translate = clipMin - dataMin * scale
```

**Example:** 100 candles at x=0..99, prices 150..200, into a pane at clipX [-0.9, 0.8], clipY [-0.9, 0.9]:

```
sx = (0.8 - (-0.9)) / (99 - 0) = 1.7 / 99 ≈ 0.01717
tx = -0.9 - 0 * sx = -0.9

sy = (0.9 - (-0.9)) / (200 - 150) = 1.8 / 50 = 0.036
ty = -0.9 - 150 * 0.036 = -6.3
```

```json
{"cmd":"setTransform","id":50,"sx":0.01717,"sy":0.036,"tx":-0.9,"ty":-6.3}
```

### Data-Space Vertex Data

Write vertices in data space. The transform handles the mapping. Candle at index 5, OHLC 160/170/155/165, half-width 0.4:

```
[5.0, 160.0, 170.0, 155.0, 165.0, 0.4]  → 24 bytes, candle6
```

The transform converts index 5 → clipX, price 160 → clipY. The shader receives clip-space coordinates.

### Shared Transforms for Synchronized Movement

Multiple DrawItems sharing the same transform move as one when you pan/zoom:

```json
{"cmd":"attachTransform","drawItemId":200,"transformId":50}
{"cmd":"attachTransform","drawItemId":201,"transformId":50}
{"cmd":"attachTransform","drawItemId":202,"transformId":50}
```

Now updating transform 50 moves all three. This is how candles + SMA + Bollinger bands stay locked together.

### Pre-Transformed Data (No Transform)

If you don't attach a transform, vertices are interpreted as clip-space directly. Useful for fixed overlays, legends, or UI elements that shouldn't move with pan/zoom.

---

## 8. Composition Patterns

### Pattern: Multi-Pane Layout

Divide the viewport into stacked regions:

```json
"panes": {
  "1": { "name": "Price",  "region": { "clipYMin": 0.15, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.85 } },
  "2": { "name": "Volume", "region": { "clipYMin": -0.35, "clipYMax": 0.10, "clipXMin": -0.95, "clipXMax": 0.85 } },
  "3": { "name": "RSI",    "region": { "clipYMin": -0.95, "clipYMax": -0.40, "clipXMin": -0.95, "clipXMax": 0.85 } }
}
```

Gaps between panes (0.10 to 0.15, -0.35 to -0.40) create visual separators. Each pane gets its own scissor, its own clear color, its own layers.

Each pane typically has its own Y transform (different data ranges), but can share the same X transform (time axis).

### Pattern: Layer Stacking

Within a pane, layers control front-to-back order:

```
Layer 10 (Grid)      ← background lines
Layer 20 (Data)      ← candlesticks, bars
Layer 30 (Overlays)  ← SMA, Bollinger bands
Layer 40 (Labels)    ← text, price markers
```

Lower layer ID = drawn first = behind. This is composable — add or remove layers without touching others.

### Pattern: Multiple DrawItems on One Buffer

A single buffer can feed multiple geometries with different vertex counts, and those geometries can feed different DrawItems. This is efficient when data shares the same buffer:

```
Buffer 100 (raw OHLC bytes)
  → Geometry 101 (candle6, 100 candles) → DrawItem 200 (instancedCandle@1)
  → Geometry 102 (candle6, 100 candles) → DrawItem 201 (instancedCandle@1, different colors)
```

Or more commonly, different buffers for different data:

```
Buffer 100 (candle data) → Geometry 101 → DrawItem 200 (candles)
Buffer 110 (SMA data)    → Geometry 111 → DrawItem 210 (SMA line)
Buffer 120 (volume data) → Geometry 121 → DrawItem 220 (volume bars)
```

### Pattern: Stencil Clipping

Render one DrawItem as a stencil mask, then clip others to that shape:

```json
{"cmd":"setDrawItemStyle","drawItemId":300,"isClipSource":true}
{"cmd":"setDrawItemStyle","drawItemId":301,"useClipMask":true}
{"cmd":"setDrawItemStyle","drawItemId":302,"useClipMask":true}
```

DrawItem 300 writes to stencil buffer but produces no visible output. DrawItems 301 and 302 only render where the stencil is set. This creates arbitrary clip shapes — circles, polygons, complex outlines.

The stencil is cleared per-pane, so clipping in one pane doesn't affect another.

### Pattern: Blend Modes for Visual Effects

```json
{"cmd":"setDrawItemStyle","drawItemId":200,"blendMode":"additive"}
```

Four modes: `normal` (default alpha blending), `additive` (glow/light effects), `multiply` (darken), `screen` (lighten). Applied per-DrawItem, so different items in the same layer can use different blends.

### Pattern: Anchored Elements

Pin a DrawItem to a pane edge so it stays fixed regardless of transform:

```json
{"cmd":"setDrawItemAnchor","drawItemId":200,"anchor":"topRight","offsetX":-10,"offsetY":-10}
```

Nine anchor points: `topLeft`, `topCenter`, `topRight`, `middleLeft`, `center`, `middleRight`, `bottomLeft`, `bottomCenter`, `bottomRight`. Offsets are in pixels.

### Pattern: Gradient Fills

Apply a gradient to any DrawItem:

```json
{"cmd":"setDrawItemGradient","drawItemId":200,"type":"linear","angle":90,"color0":{"r":0,"g":0.3,"b":0.8,"a":0.6},"color1":{"r":0,"g":0.1,"b":0.3,"a":0.0}}
```

Types: `linear` (with `angle` in degrees) and `radial` (with `center` and `radius`). Combined with `triSolid@1` area fills, this creates gradient area charts.

---

## 9. Building Non-Chart Visuals

The engine has no concept of "chart." These primitives build anything:

**Dashboard panels** — `instancedRect@1` with `cornerRadius` and gradient fills. Layer text on top with `textSDF@1`.

**Heatmaps** — Grid of `instancedRect@1` rectangles, each colored by value. No transforms needed if you pre-compute clip positions.

**Progress bars** — Two overlapping `instancedRect@1` items: background (full width) and fill (partial width). Update the fill geometry's buffer to change progress.

**Tree/graph diagrams** — `lineAA@1` for edges, `instancedRect@1` for nodes, `textSDF@1` for labels. No transforms; compute positions in clip space directly.

**Custom shapes** — Triangulate any polygon into `triSolid@1` or `triAA@1`. A circle is ~32 triangles fanning from center. A pie chart is wedge-shaped triangle fans with different colors per DrawItem.

**Gauges** — Arc segments via `triAA@1` triangulation, needle via `lineAA@1`, labels via `textSDF@1`.

---

## 10. Vertex Data Construction

### How Bytes Become Shapes

Buffers hold raw `float32` values packed tightly. The format tells the shader how to group them:

| Format | Bytes Per Unit | Fields | Used By |
|--------|---------------|--------|---------|
| pos2_clip | 8 | `[x, y]` | triSolid, triAA (alpha ignored in buffer), line2d, points |
| pos2_alpha | 12 | `[x, y, alpha]` | triAA@1 |
| pos2_color4 | 24 | `[x, y, r, g, b, a]` | triGradient@1 |
| rect4 | 16 | `[x0, y0, x1, y1]` | lineAA@1, instancedRect@1 |
| candle6 | 24 | `[x, open, high, low, close, halfWidth]` | instancedCandle@1 |
| glyph8 | 32 | `[x0, y0, x1, y1, u0, v0, u1, v1]` | textSDF@1 |
| pos2_uv4 | 16 | `[x, y, u, v]` | texturedQuad@1 |

### Constructing Common Shapes

**Connected line** from N points → `lineAA@1` needs N-1 segments:

```
for i in 0..N-2:
  segment[i] = [x[i], y[i], x[i+1], y[i+1]]   // 16 bytes each, rect4
```

vertexCount = N-1.

**Simple line** from N points → `line2d@1` needs (N-1)*2 vertices:

```
for i in 0..N-2:
  push [x[i], y[i]]    // 8 bytes, pos2_clip
  push [x[i+1], y[i+1]]
```

vertexCount = (N-1)*2. Must be even.

**Filled area** under a curve to baseline → `triSolid@1` needs (N-1)*6 vertices:

```
for i in 0..N-2:
  // Triangle 1
  push [x[i], y[i]]
  push [x[i], baseline]
  push [x[i+1], y[i+1]]
  // Triangle 2
  push [x[i+1], y[i+1]]
  push [x[i], baseline]
  push [x[i+1], baseline]
```

vertexCount = (N-1)*6. Must be multiple of 3.

**Circle** (approximate) → `triSolid@1`, fan from center:

```
for i in 0..segmentCount-1:
  angle0 = 2π * i / segmentCount
  angle1 = 2π * (i+1) / segmentCount
  push [cx, cy]
  push [cx + r*cos(angle0), cy + r*sin(angle0)]
  push [cx + r*cos(angle1), cy + r*sin(angle1)]
```

vertexCount = segmentCount * 3.

**Volume bars using candle6** — Reuse OHLC format with color trick:

```
volumeBar[i] = [
  x,                      // center X
  isUp ? 0 : volume,      // open
  volume,                  // high
  0,                       // low
  isUp ? volume : 0,       // close
  halfWidth               // bar half-width
]
```

When close >= open → colorUp (green). When close < open → colorDown (red).

---

## 11. Dynamic Updates

### Surgical Commands

Change one thing at a time. Commands merge — omitted fields keep their current values:

```json
{"cmd":"setTransform","id":50,"tx":-1.5}
```

This updates only `tx`, leaving `sx`, `sy`, `ty` unchanged.

```json
{"cmd":"setDrawItemStyle","drawItemId":200,"a":0.5}
```

This changes only the alpha channel. All other style properties remain.

### Buffer Streaming (Binary Ingest)

Append new data to a buffer without resending everything:

```
Record: [1B op] [4B bufferId LE] [4B offsetBytes LE] [4B payloadBytes LE] [payload...]
```

- **Op 1 (APPEND):** Adds payload to end. Offset is ignored.
- **Op 2 (UPDATE_RANGE):** Overwrites at offsetBytes.

After appending, update the geometry's vertex count:

```json
{"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":51}
```

For high-frequency streaming, use buffer capacity management:

```json
{"cmd":"bufferSetMaxBytes","bufferId":100,"maxBytes":240000}
{"cmd":"bufferEvictFront","bufferId":100,"bytes":2400}
{"cmd":"bufferKeepLast","bufferId":100,"bytes":240000}
```

This creates a ring-buffer effect — old data falls off, new data streams in.

### Declarative Reconciliation

Send a new SceneDocument, and the reconciler diffs it against the current state:

- New entities → create commands
- Changed properties → update commands
- Missing entities → delete commands
- Immutable property changes (format, layer name) → delete + recreate

This is efficient for bulk scene changes but has overhead for per-frame updates. Use imperative commands for live streaming, declarative for scene structure changes.

### Frame Atomicity

Wrap related changes in a frame to prevent partial renders:

```json
{"cmd":"beginFrame","frameId":1}
{"cmd":"setTransform","id":50,"tx":-1.5,"ty":0.3}
{"cmd":"setGeometryVertexCount","geometryId":101,"vertexCount":51}
{"cmd":"commitFrame","frameId":1}
```

If any command fails between begin and commit, the entire frame is poisoned and rolled back.

---

## 12. Complete Command Reference

### Scene Structure

| Command | Key Fields | Notes |
|---------|-----------|-------|
| `createPane` | id, name | Creates viewport region |
| `setPaneRegion` | id, clipYMin/YMax/XMin/XMax | Merges — omitted fields unchanged |
| `setPaneClearColor` | id, r/g/b/a, enabled | `enabled:false` disables clear |
| `createLayer` | id, paneId, name | Name is immutable |
| `createBuffer` | id, byteLength | `byteLength:0` allowed |
| `createGeometry` | id, vertexBufferId, format, vertexCount | Format is immutable |
| `createTransform` | id | Starts as identity |
| `createDrawItem` | id, layerId, name | The renderable entity |
| `delete` | id | Cascading — pane deletes its layers and their drawItems |

### Binding & Transform

| Command | Key Fields | Notes |
|---------|-----------|-------|
| `bindDrawItem` | drawItemId, pipeline, geometryId | Pipeline validation runs here |
| `attachTransform` | drawItemId, transformId | Omit transformId to detach |
| `setTransform` | id, sx/sy/tx/ty | Merges. Uses "id" not "transformId" |

### Styling

| Command | Key Fields | Notes |
|---------|-----------|-------|
| `setDrawItemColor` | drawItemId, r/g/b/a | Shorthand for primary color |
| `setDrawItemStyle` | drawItemId, [many] | Merges all: color, colorUp/Down, lineWidth, pointSize, dashLength, gapLength, cornerRadius, blendMode, isClipSource, useClipMask |
| `setDrawItemVisible` | drawItemId, visible | Show/hide without deleting |
| `setDrawItemTexture` | drawItemId, textureId | For texturedQuad@1 |
| `setDrawItemAnchor` | drawItemId, anchor, offsetX/Y | Pin to pane edge |
| `setDrawItemGradient` | drawItemId, type, angle, color0/1, center, radius | Linear or radial gradient |

### Geometry Management

| Command | Key Fields | Notes |
|---------|-----------|-------|
| `setGeometryVertexCount` | geometryId, vertexCount | Update after buffer changes |
| `setGeometryBuffer` | geometryId, vertexBufferId | Retarget to different buffer |
| `setGeometryBounds` | geometryId, minX/Y/maxX/Y | AABB for culling |
| `setGeometryIndexBuffer` | geometryId, indexBufferId | Attach index buffer |
| `setGeometryIndexCount` | geometryId, indexCount | Set index count |

### Buffer Management

| Command | Key Fields | Notes |
|---------|-----------|-------|
| `bufferSetMaxBytes` | bufferId, maxBytes | Ring-buffer capacity |
| `bufferEvictFront` | bufferId, bytes | Remove oldest bytes |
| `bufferKeepLast` | bufferId, bytes | Keep only newest bytes |

### Text & Metadata

| Command | Key Fields | Notes |
|---------|-----------|-------|
| `ensureGlyphs` | text | Pre-rasterize needed glyphs |
| `setAnnotation` | drawItemId, role/label/value | Accessibility metadata |
| `removeAnnotation` | drawItemId | Remove metadata |

### Control

| Command | Notes |
|---------|-------|
| `hello` | Echo/keepalive |
| `beginFrame` | Start atomic transaction (optional frameId) |
| `commitFrame` | Commit transaction |

---

## 13. Declarative Document Schema

A SceneDocument is a single JSON object. All sections are optional. Keys are numeric ID strings.

```json
{
  "version": 1,
  "viewport": { "width": 1280, "height": 720 },
  "buffers": {
    "100": { "byteLength": 0 }
  },
  "transforms": {
    "50": { "sx": 1, "sy": 1, "tx": 0, "ty": 0 }
  },
  "panes": {
    "1": {
      "name": "Main",
      "region": { "clipYMin": -0.95, "clipYMax": 0.95, "clipXMin": -0.95, "clipXMax": 0.95 },
      "clearColor": [0.05, 0.05, 0.08, 1.0],
      "hasClearColor": true
    }
  },
  "layers": {
    "10": { "paneId": 1, "name": "Background" },
    "20": { "paneId": 1, "name": "Data" }
  },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "rect4", "vertexCount": 20 }
  },
  "drawItems": {
    "200": {
      "layerId": 20,
      "name": "Bars",
      "pipeline": "instancedRect@1",
      "geometryId": 101,
      "transformId": 50,
      "color": [0.4, 0.7, 1.0, 0.9],
      "cornerRadius": 4.0,
      "lineWidth": 1.0,
      "pointSize": 4.0,
      "dashLength": 0.0,
      "gapLength": 0.0,
      "blendMode": "normal",
      "visible": true,
      "isClipSource": false,
      "useClipMask": false,
      "colorUp": [0, 0.8, 0, 1],
      "colorDown": [0.8, 0, 0, 1],
      "anchorPoint": "",
      "anchorOffsetX": 0,
      "anchorOffsetY": 0,
      "gradientType": "",
      "gradientAngle": 0.0,
      "gradientColor0": [1, 1, 1, 1],
      "gradientColor1": [0, 0, 0, 1],
      "gradientCenter": [0.5, 0.5],
      "gradientRadius": 0.5
    }
  }
}
```

Most DrawItem fields are optional — defaults apply. The reconciler only emits commands for fields that differ from the live scene state.

---

## 14. Edge Cases and Boundaries

### What Breaks

- **vertexCount mismatch:** Pipeline validates divisibility. triSolid@1 requires multiple of 3, line2d@1 requires multiple of 2. Points and instanced types require >= 1. Validation happens at pipeline binding time.
- **Missing buffer data:** If buffer has fewer bytes than `vertexCount * stride`, the draw call reads garbage or crashes. Keep vertex count synchronized with actual buffer contents.
- **Format change:** Geometry format is immutable. Must delete and recreate. The reconciler handles this automatically.
- **Layer/pane rename:** No rename command exists. Must delete and recreate. Cascading delete will remove child entities.
- **ID collisions:** Reusing an existing ID in a create command fails. Delete first, or use a new ID.
- **Float precision:** JSON → double → float loses bits. Use `%.9g` format and `1e-6` tolerance for comparisons. At epoch-magnitude timestamps (~1.7e9), float resolution is ~128 bytes.

### What's Graceful

- **Zero-size geometry:** A DrawItem with vertexCount=0 simply produces no draw call.
- **Invisible items:** `setDrawItemVisible` with `visible:false` skips the item entirely — no GPU cost.
- **Empty panes:** A pane with no layers or no visible DrawItems just clears and moves on.
- **Missing transform:** DrawItems without a transform use identity — vertices are interpreted as clip space.
- **Overlapping panes:** Panes can overlap. They render in ascending ID order, later panes drawing over earlier ones.

### Field Gotchas

1. **UNIFIED ID NAMESPACE:** All resource types (panes, layers, drawItems, buffers, geometries, transforms) share ONE global ID namespace via `ResourceRegistry`. If you create a layer with ID 50 and then a transform with ID 50, the transform creation **fails silently** with `"ID_TAKEN"`. Use non-overlapping ranges per type (e.g., panes 1–9, layers 10–49, transforms 60–69, buffers/geometries/drawItems 100+).
2. **PaneRegion order:** `clipYMin, clipYMax, clipXMin, clipXMax` — Y before X
3. **setTransform field:** Uses `"id"`, NOT `"transformId"`
4. **createGeometry field:** Uses `"vertexBufferId"`, NOT `"bufferId"`
5. **setGeometryVertexCount field:** Uses `"geometryId"`, NOT `"id"`
6. **createBuffer requires:** `"byteLength"` field (can be 0)
7. **Colors:** RGBA in [0, 1] range — not 0–255

---

## 15. Themes

Six built-in theme presets: **Dark**, **Light**, **Midnight**, **Neon**, **Pastel**, **Bloomberg**.

Applied via `ThemeManager::applyTheme()` with a `ThemeTarget` that maps scene IDs to theme roles. Themes set: backgroundColor, candleUp/Down, volumeUp/Down, gridColor, tickColor, labelColor, crosshairColor, textColor, highlightColor, drawingColor, overlayColors[8], gridDashLength/gapLength/opacity, paneBorderColor/width, separatorColor/width.

Themes are not magic — they just emit `setDrawItemStyle`, `setPaneClearColor`, etc. commands for the mapped IDs. You can apply a theme and then override individual styles.

---

## 16. Thinking Process for "I Want to Render X"

1. **What shapes?** Decompose the visual into triangles, lines, rectangles, points, or text. Most things are combinations of these.

2. **How many panes?** If regions need independent clipping or backgrounds, they're separate panes. If they share the same space, they go in layers within one pane.

3. **What coordinate space?** If the data has a natural range (time × price), use transforms. If positions are fixed (UI, legends), write directly in clip space.

4. **Layer order?** Background → data → foreground → labels. Assign layer IDs accordingly.

5. **Shared or independent transforms?** Things that pan/zoom together share a transform. Fixed elements get no transform or their own.

6. **How will it update?** Streaming data → binary ingest + vertex count updates. User interaction → transform updates. Structure changes → reconciler.

7. **What styling?** Color, alpha, blend mode, dash pattern, corner radius, gradient, clipping mask — all composable per DrawItem.

This process works for candlestick charts, organizational diagrams, heatmaps, game UIs, or anything else that can be expressed as positioned, styled, layered 2D geometry.

---

## 15. Pure JSON Workflow (D79)

The `dc_json_host` binary eliminates the need for per-chart C++ code. A single `.json` file is the only input — no compilation required.

### Running a Chart

```bash
# Direct rendering (outputs TEXT/FRME frames to stdout):
echo '{"cmd":"render"}' | build/core/dc_json_host charts/candle_chart.json > /tmp/frame.bin

# Interactive via live-viewer (opens at http://localhost:3000):
DC_CHART=charts/candle_chart.json node apps/live-viewer/server.mjs
```

### Extended SceneDocument Format

The JSON host uses the standard `SceneDocument` format with three extensions:

#### 1. Inline Buffer Data

Buffers can include vertex float data directly in JSON. When `data` is present, `byteLength` is derived automatically.

```json
"buffers": {
  "100": {
    "data": [0.0, 0.5, 0.5, -0.5, -0.5, -0.5]
  }
}
```

This replaces the binary ingest step — the host uploads inline data to IngestProcessor on startup.

#### 2. Viewport Declarations

Viewports define interactive pan/zoom regions tied to panes and transforms. Viewports in the same `linkGroup` share X-axis pan/zoom.

```json
"viewports": {
  "price": {
    "transformId": 50, "paneId": 1,
    "xMin": 0, "xMax": 100, "yMin": 96, "yMax": 114,
    "linkGroup": "time"
  },
  "volume": {
    "transformId": 51, "paneId": 2,
    "xMin": 0, "xMax": 100, "yMin": 0, "yMax": 4000,
    "linkGroup": "time",
    "panY": false, "zoomY": false
  }
}
```

| Field | Default | Description |
|-------|---------|-------------|
| `transformId` | required | Which scene transform to update on pan/zoom |
| `paneId` | required | Which pane this viewport corresponds to |
| `xMin/xMax/yMin/yMax` | 0/1/0/1 | Initial data range |
| `linkGroup` | `""` | Name of pan/zoom link group |
| `panX/panY` | `true` | Whether panning is allowed on each axis |
| `zoomX/zoomY` | `true` | Whether zooming is allowed on each axis |

When the user presses `Home`, all viewports reset to their initial ranges.

#### 3. Text Overlay

Text labels rendered by the browser overlay (not by GL). Positions are in clip space.

```json
"textOverlay": {
  "fontSize": 13,
  "color": "#b2b5bc",
  "labels": [
    { "clipX": 0.0, "clipY": 0.9, "text": "BTCUSD · 1H", "align": "c" },
    { "clipX": -0.95, "clipY": -0.35, "text": "Volume", "align": "l", "color": "#666" }
  ]
}
```

| Field | Default | Description |
|-------|---------|-------------|
| `fontSize` | `12` | Default font size (pixels, physical) |
| `color` | `"#b2b5bc"` | Default label color (hex) |
| `labels[].clipX/clipY` | `0` | Position in clip space [-1, 1] |
| `labels[].text` | `""` | Label text |
| `labels[].align` | `"l"` | `"l"` left, `"c"` center, `"r"` right |
| `labels[].color` | (overlay default) | Per-label color override |
| `labels[].fontSize` | (overlay default) | Per-label font size override |

Clip-to-pixel conversion: `px = (clipX+1)/2 × W`, `py = (1−clipY)/2 × H`.

### Complete Self-Contained Example

A minimal interactive candle chart in one JSON file:

```json
{
  "version": 1,
  "viewport": { "width": 900, "height": 600 },
  "buffers": {
    "100": { "data": [0,100,103,98,102,0.35, 1,102,105,101,104,0.35, 2,104,106,103,105,0.35] },
    "50": { "byteLength": 0 }
  },
  "transforms": {
    "50": { "tx": 0, "ty": 0, "sx": 1, "sy": 1 }
  },
  "panes": {
    "1": {
      "name": "price",
      "hasClearColor": true,
      "clearColor": [0.11, 0.11, 0.14, 1.0]
    }
  },
  "layers": {
    "10": { "paneId": 1, "name": "candles" }
  },
  "geometries": {
    "101": { "vertexBufferId": 100, "format": "candle6", "vertexCount": 3 }
  },
  "drawItems": {
    "102": {
      "layerId": 10,
      "pipeline": "instancedCandle@1",
      "geometryId": 101,
      "transformId": 50,
      "colorUp": [0.212, 0.757, 0.408, 1.0],
      "colorDown": [0.839, 0.267, 0.267, 1.0]
    }
  },
  "viewports": {
    "price": {
      "transformId": 50, "paneId": 1,
      "xMin": -1, "xMax": 4, "yMin": 95, "yMax": 110
    }
  },
  "textOverlay": {
    "fontSize": 14,
    "color": "#b2b5bc",
    "labels": [
      { "clipX": 0.0, "clipY": 0.9, "text": "3 Candles", "align": "c" }
    ]
  }
}
```

### Reference Chart Files

| File | Description |
|------|-------------|
| `charts/simple_triangle.json` | Minimal: one pane, one triangle, text label |
| `charts/candle_chart.json` | Two-pane candle+volume with linked viewports |
| `charts/dashboard.json` | Four-pane dashboard: candles, SMA, volume, RSI, MACD |

### Input Protocol

The JSON host accepts the same stdin commands as the existing demo servers:

- `{"cmd":"render"}` — re-render current frame
- `{"cmd":"mouse","x":N,"y":N,"buttons":N,"type":"down|move|up"}` — mouse interaction
- `{"cmd":"scroll","x":N,"y":N,"dy":N}` — zoom via scroll
- `{"cmd":"key","code":"ArrowLeft|ArrowRight|ArrowUp|ArrowDown|Home"}` — keyboard
- `{"cmd":"resize","w":N,"h":N}` — resize framebuffer
