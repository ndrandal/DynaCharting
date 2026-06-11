---
title: ECG monitor — PQRST trace
referenceTool: medical / ECG monitor
tier: composed
---

A medical **ECG monitor** trace, drawn cross-domain by the very same vector renderer that draws the equities views. The 250 Hz, 72 bpm PQRST waveform from the synthetic dataset (`apps/showcase/data/synthetic/ecg.json`) is baked once into a `line2d@1` vertex buffer and framed by a single affine transform — a clean green-on-dark heartbeat, no markets in sight.

| | |
|---|---|
| **DATA** | synthetic ECG · 250 Hz · 72 bpm · first ~8 s (2000 samples, ~10 beats) |
| **PIPELINE** | `line2d@1` (LineList) |
| **WRITE MODE** | static — baked into manifest `uploads` (no live replay) |
| **BUFFERS** | `500` pos2_clip trace (segment-pair vertices) |
| **SOURCE** | synthetic dataset → baked at build time (no capture/embassy) |

**What's going on (the technique).** This is a STATIC computed view. There is no live data path: the amplitude series is read from the dataset at manifest-build time, expanded into `pos2_clip` vertices in *data space* (`x = sampleIndex`, `y = amplitude`), and frozen into the manifest's `uploads`. Because `line2d@1` is a **LineList** pipeline (GL_LINES — discrete segments, not a strip), the polyline is materialized as consecutive segment pairs `(p_i, p_{i+1})`, so 2000 samples become 3998 vertices. One baked transform maps data → clip: `x` over the 8 s window, `y` over the amplitude range `[-0.32, 1.16]` with headroom kept so the R-spike clears the top.

**The cross-domain point.** Nothing in the renderer knows or cares that this is a heartbeat. The geometry is domain-agnostic: feed it OHLC and you get candles; feed it a PQRST waveform and you get a hospital monitor. The line2d pipeline is 1px (WebGPU LineList has no native width control), which suits a crisp clinical trace.
