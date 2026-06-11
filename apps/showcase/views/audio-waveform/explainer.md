---
title: Audio waveform — mirrored amplitude
referenceTool: audio editor waveform
tier: composed
---

The classic **audio-editor waveform** — a dense, mirrored amplitude envelope — drawn cross-domain by the engine's gradient-fill pipeline. The same `triGradient@1` that shades market areas here paints a synthesized audio track: a noisy carrier with a sustained mid swell and a handful of transient attack/decay "bursts" (the silhouette of a drum-driven mix), filled top-to-bottom and brightened at the peaks.

| | |
|---|---|
| **DATA** | synthesized amplitude envelope · 480 columns · deterministic |
| **PIPELINE** | `triGradient@1` (Pos2Color4, per-vertex color) |
| **WRITE MODE** | static — baked into manifest `uploads` (no live replay) |
| **BUFFERS** | `500` pos2_color4 mirrored-area triangles |
| **SOURCE** | synthesized at build time (no capture/embassy) |

**What's going on (the technique).** This is a STATIC computed view. The amplitude envelope is synthesized deterministically at manifest-build time, then materialized as a **mirrored filled area**: each column quad spans `+amp .. -amp` and is split into two triangles, so 480 columns become 2874 vertices. Because `triGradient@1` carries **per-vertex** color (`Pos2Color4` = x, y, r, g, b, a), the fill can grade teal → cyan toward the peaks and dim slightly toward the mirror — giving the body depth with zero shader code. The vertices are authored directly in clip space, so no transform is needed; the geometry is its own framing.

**The cross-domain point.** A waveform is just a filled polygon with a clever color ramp — and the engine already draws filled, gradient-shaded polygons natively. Feed the same pipeline an audio envelope instead of a market series and you get a DAW-style waveform, no bespoke audio code in the renderer.
