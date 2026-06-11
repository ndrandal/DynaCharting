---
title: Contour — iso-bands
referenceTool: Topographic isolines
tier: walled
---

A topographic contour map — **marching-squares** run on a scalar field (the `heatmap.json` 16×16 grid, bilinearly upsampled to a smooth surface), quantized into filled iso-bands with crisp white isolines drawn at each band boundary, rasterized to a topographic colormap and drawn as a single textured quad. **Shown precomputed** — the isoline extraction runs once on the CPU at build time; doing it live on the GPU is the frontier.

| | |
|---|---|
| **DATA** | `heatmap.json` scalar field (16×16, last frame) → upsampled 256×256 surface |
| **PIPELINE** | `texturedQuad@1` (one full-pane quad, `pos2_uv4`) |
| **WRITE MODE** | static — RGBA8 colormap uploaded once via `setTexturePixels` (ENC-532) |
| **TEXTURE** | `80` · 256×256 RGBA8 · 9 iso-bands (topographic ramp) + white isolines |
| **THE WALL** | **live GPU isoline extraction** — marching-squares in a compute shader, re-meshed every frame |
| **TECHNIQUE NOTE** | option (a): filled iso-bands rasterized to a texture (vs. emitting isolines as `line2d` uploads) |

**What's going on.** A contour map answers "where is the field equal to *this* level?". The classic algorithm is **marching-squares**: walk the grid cell by cell, classify each corner above/below a threshold, and emit the line segments that cross the cell — repeated per iso-level to build the isolines. Here that work is done **on the CPU at build time**: the scalar field is quantized into 9 bands, the band index is colored through a topographic ramp, and a crisp isoline is drawn wherever the quantized band changes between neighbours (the extracted contour). The result is baked into a 256×256 RGBA8 texture and handed to the engine via `setTexturePixels`; the manifest just declares a unit quad on `texturedQuad@1`. We chose **option (a)** — rasterize the filled bands to a texture — over option (b) — emit the extracted isolines as `line2d` static uploads — because the filled topographic look reads better and keeps all three walled views on one proven texturedQuad path.

> **THE WALL (the frontier this view documents).** The live version runs **marching-squares on the GPU**: a compute shader extracts the iso-segments from the field every frame and re-meshes the contour as the field changes (or a fragment shader evaluates the band/isoline analytically per pixel) — no CPU prepass, no precomputed texture. That live GPU isoline extraction is the frontier tier; this card proves the *output*, the wall is the *live extraction*.
