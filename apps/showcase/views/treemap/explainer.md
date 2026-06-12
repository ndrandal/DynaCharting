---
title: Treemap — market map
referenceTool: Finviz market map
tier: composed
---

A Finviz-style market map: the four symbols (AAPL/MSFT/NVDA/TSLA) are sectors sized by traded volume, each subdivided into five time-bucket leaf tiles sized by their own volume share and colored green-to-red by price performance. The nested rectangle layout is **re-tessellated at N timesteps** by a squarified-treemap pass whose per-symbol/per-leaf weights drift along the dataset's volume time-series — so the tiles **resize and re-pack live** as the replay advances.

| | |
|---|---|
| **DATA** | AAPL/MSFT/NVDA/TSLA · windowed volume (size) + close/open perf (color) |
| **TECHNIQUE** | geometry-frame replay — per-timestep squarified re-layout → `UPDATE_RANGE` frames |
| **PIPELINE** | `triGradient@1` (per-vertex RGBA color) |
| **GEOMETRY** | `pos2_color4` triangles — 20 leaf tiles × 2 tris = 120 verts (constant) |
| **BUFFERS** | `601` pos2_color4 tiles — pre-sized, overwritten in place each frame |
| **COLOR** | per-vertex — green gains / red losses, brightness ∝ magnitude |

**What's going on (the technique).** A treemap can't ride the instanced-rect/per-item-color path the order-book views use, because each tile needs its OWN color. So this view tessellates to the `pos2_color4` format: every leaf rect becomes two triangles whose vertices each carry position AND an RGBA color. The layout is a squarified treemap (Bruls/Huizing/van Wijk) run twice — once to pack the four sectors into the unit square minimising aspect ratio, then recursively to pack each sector's leaves. To make it **LIVE** (ENC-580), the generator re-runs that whole layout at 40 timesteps with a moving emphasis window over the 40 volume buckets, and emits each timestep's full tile geometry as a single `UPDATE_RANGE` record (op 2, offset 0) that overwrites the entire pre-sized tile buffer. The tile *count* never changes (tiles resize, the buffer is stable), so the replay just streams these full-buffer overwrites; the `triGradient` backend re-reads and redraws the buffer each frame, and the market map re-lays-out in real time.
