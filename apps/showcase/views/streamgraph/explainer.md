---
title: Streamgraph — volume flow
referenceTool: streamgraph / stacked area
tier: composed
---

A streamgraph of traded volume: each symbol's per-bucket volume is one band, the four bands stacked and centered on a wandering "wiggle" baseline (`g0 = -½·Σ values`) so the silhouette flows organically about y=0 rather than sitting on a flat axis. Every band ribbon is **tessellated at manifest-build time** from the market datasets and embedded as static manifest `uploads` — no streaming, capture, or replay.

| | |
|---|---|
| **DATA** | AAPL/MSFT/NVDA/TSLA · volume per bucket (40 time steps) |
| **TECHNIQUE** | build-time stacking + wiggle baseline → static `uploads` |
| **PIPELINE** | `triGradient@1` (per-vertex RGBA color) |
| **GEOMETRY** | `pos2_color4` triangle strips — one ribbon per band over time |
| **BUFFERS** | `601` pos2_color4 bands (one static upload) |
| **COLOR** | per-vertex — per-band hue with a gentle left→right brightness flow |

**What's going on (the technique).** A streamgraph needs each band to be an arbitrary filled ribbon with its own color — not a row of uniform-color rects — so it tessellates to the `pos2_color4` format: for every time step, the band's lower and upper stacked offsets become a quad (two triangles) whose vertices carry both position and the band's hue. The stack is computed at build time with a centered ThemeRiver "wiggle" baseline, giving the flowing organic silhouette; emitting all four bands into one buffer lets a single `triGradient@1` draw item render the whole stream. The shape is resolved deterministically at build time from the volume series, with no upstream feed.
