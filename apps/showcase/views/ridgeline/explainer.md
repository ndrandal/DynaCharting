---
title: Ridgeline — density bands
referenceTool: D3 / Observable ridgeline
tier: composed
---

A ridgeline plot of price distributions: each symbol's lastPrice ticks are histogrammed over its own range and Gaussian-smoothed into a density curve, then the four bands are stacked with vertical offsets and allowed to overlap so each ridge partly occludes the one behind it. Every band's filled area is **tessellated at manifest-build time** from the market datasets and embedded as static manifest `uploads` — no streaming, capture, or replay.

| | |
|---|---|
| **DATA** | AAPL/MSFT/NVDA/TSLA · lastPrice (200 ticks each) → 48-bin KDE |
| **TECHNIQUE** | build-time histogram + Gaussian smooth → static `uploads` |
| **PIPELINE** | `triGradient@1` (per-vertex RGBA color) |
| **GEOMETRY** | `pos2_color4` triangle strips (baseline → density curve) |
| **BUFFERS** | `601` pos2_color4 bands (one static upload) |
| **COLOR** | per-vertex vertical gradient — dark baseline → bright crest, per-band hue |

**What's going on (the technique).** A filled, gradient-shaded area can't be expressed by the single-uniform-color instanced-rect path — it needs color to vary *within* the shape. So each density band is tessellated to the `pos2_color4` format: a triangle strip between the band's baseline and its smoothed curve, where the baseline vertices carry a dark translucent color and the crest vertices carry the band's bright hue, giving a vertical gradient fill the rasterizer interpolates. The four bands are emitted back-to-front into one buffer so the nearer ridge overlaps the one above, and a single `triGradient@1` draw item renders all of them — the classic D3/Observable ridgeline, computed deterministically at build time from the price distributions with no upstream feed.
