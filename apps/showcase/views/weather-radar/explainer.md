---
title: Weather radar — intensity field
referenceTool: weather radar / NEXRAD
tier: composed
---

A **NEXRAD-style weather radar** sweep — a 2D intensity field rendered as a heatmap. The 16×16 scalar field from the synthetic dataset (`apps/showcase/data/synthetic/heatmap.json`) is rasterized into an upscaled RGBA8 radar colormap and blitted onto a single `texturedQuad@1`. This is the cross-domain face of the same texture-feed path a market heatmap or spectrogram would use.

| | |
|---|---|
| **DATA** | synthetic 16×16 scalar field · representative t=0 frame (full [0,1] range) |
| **PIPELINE** | `texturedQuad@1` (Pos2Uv4 quad + sampled RGBA8 texture) |
| **WRITE MODE** | static — baked into manifest `uploads` + `textures` (no live replay) |
| **BUFFERS** | `500` pos2_uv4 quad · texture `700` (192×192 RGBA8) |
| **SOURCE** | synthetic dataset → rasterized at build time (no capture/embassy) |

**What's going on (the technique).** This view exercises the **ViewTexture escape hatch** (ENC-532). The `texturedQuad@1` pipeline can't compute per-pixel color from a JSON manifest — that's the frontier wall — so the **colormap is rasterized upstream**, at manifest-build time: the 16×16 field is bilinearly upscaled to 192×192 and run through a NEXRAD-style palette (dark → green → yellow → orange → red → magenta), producing RGBA8 bytes that are base64'd into `manifest.textures`. At load time `EngineHost.setTexturePixels` uploads those bytes; the draw item binds the texture by id via `setDrawItemTexture`, and the pipeline simply **blits** the colormap onto a clip-space quad. Engine purity is preserved — it samples a pre-colored texture, it does not invent color.

**The cross-domain point.** A weather-radar sweep, a spectrogram, and a market depth heatmap are the same primitive: a colormapped scalar field on a quad. The producer rasterizes; the engine blits. Swap the field's domain and nothing in the render path changes.
