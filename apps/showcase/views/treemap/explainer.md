---
title: Treemap — market map
referenceTool: Finviz market map
tier: composed
---

A Finviz-style market map: the four symbols (AAPL/MSFT/NVDA/TSLA) are sectors sized by total traded volume, each subdivided into five time-bucket leaf tiles sized by their own volume share and colored green-to-red by price performance over the window. The entire nested rectangle layout is **tessellated at manifest-build time** by a squarified-treemap pass over the market datasets, then embedded as static manifest `uploads` — no streaming, capture, or replay.

| | |
|---|---|
| **DATA** | AAPL/MSFT/NVDA/TSLA · volume (size) + close/open perf (color) |
| **TECHNIQUE** | build-time squarified treemap → static `uploads` (no replay) |
| **PIPELINE** | `triGradient@1` (per-vertex RGBA color) |
| **GEOMETRY** | `pos2_color4` triangles — 20 leaf tiles × 2 tris = 120 verts |
| **BUFFERS** | `601` pos2_color4 tiles (one static upload) |
| **COLOR** | per-vertex — green gains / red losses, brightness ∝ magnitude |

**What's going on (the technique).** A treemap can't ride the instanced-rect/per-item-color path the order-book views use, because each tile needs its OWN color. So this view tessellates to the `pos2_color4` format: every leaf rect becomes two triangles whose vertices each carry position AND an RGBA color baked from the leaf's performance. The layout itself is a squarified treemap (Bruls/Huizing/van Wijk) run twice — once to pack the four sectors into the unit square minimising aspect ratio, then recursively to pack each sector's leaves into its sub-rect. Because the color travels in the vertex buffer, a single `triGradient@1` draw item renders all 20 tiles with independent heat — the Finviz market-map look, computed deterministically at build time with no upstream feed.
