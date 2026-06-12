---
title: Audio waveform — mirrored amplitude
referenceTool: audio editor waveform
tier: composed
---

The classic **audio-editor waveform** — a dense, mirrored amplitude envelope — drawn cross-domain by the engine's gradient-fill pipeline, and **streamed live**. The same `triGradient@1` that shades market areas here paints a synthesized audio track: a few mixed sinusoids (a 220 Hz carrier + harmonics + vibrato) under a swelling/decaying amplitude envelope of speech/music-like "bursts", filled top-to-bottom and brightened at the peaks — and it fills IN left→right over ~20s like a track being recorded.

| | |
|---|---|
| **DATA** | synthesized amplitude envelope · 480 columns · deterministic |
| **PIPELINE** | `triGradient@1` (Pos2Color4, per-vertex color) |
| **WRITE MODE** | streaming — grown column-by-column via `records.json` replay (no `uploads`) |
| **BUFFERS** | `500` pos2_color4 mirrored-area triangles (grows over the timeline) |
| **SOURCE** | synthesized in `records.gen.mjs` (no capture/embassy) |

**What's going on (the technique).** This is a STREAMING computed view. The amplitude envelope is synthesized deterministically in `records.gen.mjs`, then materialized as a **mirrored filled area**: each column quad spans `+amp .. -amp` and is split into two triangles, so 480 columns become 2874 vertices. But the geometry is no longer baked — the band is **streamed column-by-column** as `pos2_color4` triangles APPENDED to buffer `500` over the recorded timeline (100 frames / ~20s). As records land, the replay engine advances `vertexCount = bytes / 24`, so the filled envelope grows to the right (the same live data path the candle / ecg / scatter views use; ENC-569 re-reads the grown buffer and redraws). Because `triGradient@1` carries **per-vertex** color (`Pos2Color4` = x, y, r, g, b, a), the fill grades teal → cyan toward the peaks and dims toward the mirror — depth with zero shader code. Vertices are authored directly in clip space, so no transform is needed; the geometry is its own framing.

**Why a filled area, not `line2d@1`.** `line2d@1` is a 1px WebGPU LineList that draws disconnected vertex pairs — a thin, broken trace (ENC-587). A waveform envelope reads far more boldly as a filled polygon, and it sidesteps the line pipeline entirely.

**The cross-domain point.** A waveform is just a filled polygon with a clever color ramp — and the engine already draws filled, gradient-shaded polygons natively. Stream the same pipeline an audio envelope instead of a market series and you get a DAW-style waveform that draws live, no bespoke audio code in the renderer.
