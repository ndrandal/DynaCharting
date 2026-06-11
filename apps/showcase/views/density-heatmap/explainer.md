---
title: Density Heatmap — KDE glow
referenceTool: Bookmap liquidity heatmap
tier: walled
---

A Bookmap-style liquidity heatmap — a 2D **Gaussian-KDE density field** over a scattered point cloud, rasterized to a dark→cyan→white *glow* colormap and drawn as a single textured quad straight from the JSON manifest. The bright hot zones are where the kernel-density estimate piles up (clustered "resting liquidity"); the cool wash is the diffuse background. **Shown precomputed** — the density field is computed once on the CPU at build time; doing it live per-pixel on the GPU is the frontier.

| | |
|---|---|
| **DATA** | synthetic point cloud (5 clusters + diffuse bg, ~3.8k points) → 256×256 KDE field |
| **PIPELINE** | `texturedQuad@1` (one full-pane quad, `pos2_uv4`) |
| **WRITE MODE** | static — RGBA8 colormap uploaded once via `setTexturePixels` (ENC-532) |
| **TEXTURE** | `70` · 256×256 RGBA8 glow colormap (dark navy → cyan → white-hot) |
| **THE WALL** | **live per-pixel GPU density accumulation + glow** — a custom KDE compute/fragment shader |

**What's going on.** A liquidity heatmap is a *density field*: every observation (an order, a print) deposits a small Gaussian kernel, and the field is the sum of all those kernels — bright where they overlap, dark where they don't. Here the engine renders the field faithfully, but as a **precomputed RGBA8 texture**: the kernel-density estimate is accumulated on the CPU at build time (a separable Gaussian stamp splatted at each of ~3,800 points), gamma-corrected for glow, mapped through the colormap, and handed to the pipeline as bytes via the `setTexturePixels` escape hatch. The manifest itself only declares a pane, a unit quad, and `texturedQuad@1` — the engine just blits the colormap; it computes no per-pixel density.

> **THE WALL (the frontier this view documents).** The live version computes the KDE *on the GPU, per pixel, every frame*: each fragment accumulates the Gaussian contribution of every nearby point (or reads a GPU-built density buffer) and applies the glow tone-map in a **custom fragment/compute shader** — no CPU prepass, no precomputed texture. That live-GPU density accumulation + glow is the frontier tier; this card proves the *output*, the wall is the *live compute*.
