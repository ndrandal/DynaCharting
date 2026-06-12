---
title: ECG monitor — PQRST trace
referenceTool: medical / ECG monitor
tier: composed
---

A medical **ECG monitor** trace, drawn cross-domain by the very same vector renderer that streams the equities views. The 250 Hz, 72 bpm PQRST waveform from the synthetic dataset (`apps/showcase/data/synthetic/ecg.json`) is streamed sample-by-sample into a growing `lineAA@1` rect4 buffer and framed by a single affine transform — a bold, anti-aliased green-on-dark heartbeat that draws live like a hospital monitor, no markets in sight.

| | |
|---|---|
| **DATA** | synthetic ECG · 250 Hz · 72 bpm · 5000 samples over 20 s |
| **PIPELINE** | `lineAA@1` (instanced thick AA segments) |
| **WRITE MODE** | streaming — buffer grown sample-by-sample via the live replay path (no `uploads`) |
| **BUFFERS** | `500` rect4 trace (grows; one connected segment `[prevPt → newPt]` per sample) |
| **SOURCE** | synthetic dataset → `records.gen.mjs` → `records.json` (replayed over the data plane) |

**What's going on (the technique).** This is a STREAMING computed view. There is no baked geometry: the trace buffer starts empty and `records.json` grows it sample-by-sample over the recorded timeline, the SAME live data path the candle views use. Each NEW sample becomes ONE `rect4` segment — `[x0 = prev sampleIndex, y0 = prev amplitude, x1 = this sampleIndex, y1 = this amplitude]`, 16 bytes — appended to the buffer. Consecutive segments share an endpoint (`segment i.p1 == segment i+1.p0`), so the appended instances form a single CONNECTED polyline. As records land, the replay engine's GrowthSync advances the geometry's instance count (`bytes / 16`) so the line lengthens to the right. One baked transform maps data → clip: `x` over the 8 s window, `y` over the amplitude range with headroom kept so the R-spike clears the top.

**Thick, anti-aliased trace (lineAA@1).** `lineAA@1` is a WebGPU **instanced** pipeline: each `rect4` segment is expanded into a thick, anti-aliased quad in the vertex shader (here ~3px), so the streamed connected segments render as a bold, continuous waveform rather than the thin, broken segment-pairs of the old `line2d@1` LineList. The lineAA backend re-counts its instance count (`bufferBytes / 16`) on each buffer-version bump (ENC-569), so the streaming APPEND growth path drives the live scroll directly.

**The cross-domain point.** Nothing in the renderer knows or cares that this is a heartbeat. The geometry is domain-agnostic: stream it OHLC and you get candles; stream it a PQRST waveform and you get a hospital monitor. `lineAA@1` gives the trace a proper clinical-monitor stroke with smooth anti-aliased edges.
