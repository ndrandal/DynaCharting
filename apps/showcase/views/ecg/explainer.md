---
title: ECG monitor — PQRST trace
referenceTool: medical / ECG monitor
tier: composed
---

A medical **ECG monitor** trace, drawn cross-domain by the very same vector renderer that streams the equities views. The 250 Hz, 72 bpm PQRST waveform from the synthetic dataset (`apps/showcase/data/synthetic/ecg.json`) is streamed sample-by-sample into a growing `line2d@1` vertex buffer and framed by a single affine transform — a clean green-on-dark heartbeat that draws live like a hospital monitor, no markets in sight.

| | |
|---|---|
| **DATA** | synthetic ECG · 250 Hz · 72 bpm · 5000 samples over 20 s |
| **PIPELINE** | `line2d@1` (LineList) |
| **WRITE MODE** | streaming — buffer grown sample-by-sample via the live replay path (no `uploads`) |
| **BUFFERS** | `500` pos2_clip trace (grows; `x = sampleIndex`, `y = amplitude`) |
| **SOURCE** | synthetic dataset → `records.gen.mjs` → `records.json` (replayed over the data plane) |

**What's going on (the technique).** This is a STREAMING computed view. There is no baked geometry: the trace buffer starts empty and `records.json` grows it sample-by-sample over the recorded timeline, the SAME live data path the candle views use. Each sample becomes ONE `pos2_clip` vertex — `x = sampleIndex` (static), `y = amplitude` (the streamed value), 8 bytes — appended to the buffer; as records land, the replay engine's GrowthSync advances the geometry's vertex count (`bytes / 8`) so the line lengthens to the right. This is the same "compound-line stride-8" trick the candle-overlays SMA uses. One baked transform maps data → clip: `x` over the 8 s window, `y` over the amplitude range with headroom kept so the R-spike clears the top.

**Pending ENC-569 (line2d growth).** `line2d@1` is a **LineList** pipeline (GL_LINES — discrete segment pairs, not a strip), so the growing one-vertex-per-sample trace becomes a fully connected, live-scrolling polyline only once **ENC-569** lands the line2d / vertex-buffer growth fix — the very same limitation that keeps the candle-overlays SMA captured-but-not-drawn. The streaming records + manifest + growth descriptor are authored here; the live animation switches on when ENC-569 merges.

**The cross-domain point.** Nothing in the renderer knows or cares that this is a heartbeat. The geometry is domain-agnostic: stream it OHLC and you get candles; stream it a PQRST waveform and you get a hospital monitor. The line2d pipeline is 1px (WebGPU LineList has no native width control), which suits a crisp clinical trace.
