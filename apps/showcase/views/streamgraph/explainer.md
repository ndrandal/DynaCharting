---
title: Streamgraph — bands flow over time
referenceTool: streamgraph / stacked area
tier: composed
---

A LIVE streamgraph: six bands stacked and centered on a wandering "wiggle" baseline (`g0 = -½·Σ values`) so the silhouette flows organically about y=0 rather than sitting on a flat axis. The band thicknesses **evolve over time** — every timestep the bands are re-evaluated (a smooth sum-of-sines), re-stacked, and re-tessellated, then replayed as an `UPDATE_RANGE` that overwrites the band vertex buffer in place, so the river **flows** and the silhouette undulates.

| | |
|---|---|
| **DATA** | six synthetic flow series · 40 time buckets, 60 frames over 20s |
| **TECHNIQUE** | per-frame stacking + wiggle baseline → geometry-frame replay |
| **PIPELINE** | `triGradient@1` (per-vertex RGBA color) |
| **GEOMETRY** | `pos2_color4` triangles — one ribbon per band over time |
| **BUFFERS** | `601` pos2_color4 bands (pre-sized, full-buffer `UPDATE_RANGE`/frame) |
| **COLOR** | per-vertex — per-band hue with a gentle left→right brightness flow |

**What's going on (the technique).** A streamgraph needs each band to be an arbitrary filled ribbon with its own color — not a row of uniform-color rects — so it tessellates to the `pos2_color4` format: for every time step the band's lower and upper stacked offsets become a quad (two triangles) whose vertices carry both position and the band's hue. The stack is computed with a centered ThemeRiver "wiggle" baseline, giving the flowing organic silhouette; emitting all bands into one buffer lets a single `triGradient@1` draw item render the whole stream. To make it LIVE, `records.gen.mjs` re-runs that math at 60 timesteps with each band's thickness drifting as a smooth, looping sum-of-sines, and `records.json` carries one full-buffer `UPDATE_RANGE` per frame. The vertex count is constant (6 bands × 39 segments × 6 verts), so the buffer is pre-sized once and each frame is a stable in-place overwrite; ENC-569's triGradient backend re-reads and redraws the updated buffer every frame, so the river visibly flows.
