---
title: Correlation Heatmap — texture escape hatch
referenceTool: correlation matrix
tier: composed
---

An N×N correlation heatmap — each cell coloured by the pairwise correlation of two symbols' return series on a diverging blue↔red scale (blue = anti-correlated, white ≈ uncorrelated, red = correlated; the diagonal is +1). Per-cell colour is the wall the instanced path hits — one draw item carries one uniform colour — so the colormap is **rasterized upstream into an RGBA8 texture** at manifest-build time and blitted onto a pane-covering quad by the `texturedQuad@1` pipeline (the **ViewTexture escape hatch**, ENC-532). It now **animates**: a rolling-window correlation is rasterized into a short sequence of frames the replay engine swaps over time (ENC-568 texture track).

| | |
|---|---|
| **DATA** | 4×4 Pearson correlation of AAPL/MSFT/NVDA/TSLA per-bucket log returns |
| **PIPELINE** | `texturedQuad@1` (one quad instance, samples a colormap texture) |
| **WRITE MODE** | animated `textures` track (setTexturePixels per frame) + a one-quad `uploads` |
| **COMPOSED VIA** | rolling-window matrix → diverging colormap → 128×128 RGBA8 raster → texture blit |
| **TEXTURE** | `700` · 128×128 RGBA8 · 11 frames @ 1s, looping (4×4 matrix upscaled, with cell gridlines) |
| **SOURCE** | precomputed from `data/market/*` by `.build-composed-static.mjs` |

**What's going on (the technique).** The instanced and triangle pipelines colour a whole draw item with one uniform colour — they can't give every grid cell its own colour from a JSON manifest. The escape hatch is to compute the per-cell colours upstream and ship them as pixels: `.build-composed-static.mjs` computes the correlation matrix, maps each cell through a diverging blue↔red colormap, nearest-neighbour-upscales it to a crisp RGBA8 image (with thin cell gridlines), and base64-encodes it. The manifest carries a base image in `textures[]` (applied with `setTexturePixels` on load), and the same build step writes a **rolling-window** sequence of frames into `records.json` as a `textures` track — a 16-sample correlation window slid across the tape. The replay engine swaps the bound texture (`700`) at each frame's timestamp (1s apart, looping), so the heatmap **evolves** as the window moves. A single `texturedQuad@1` instance covering the pane samples the current texture via `setDrawItemTexture`. The engine stays pure — it only blits whichever producer-rasterized texture is current; all the per-cell colour logic lives in the build step.

> **Correlation values** (diverging colormap): the 4-symbol correlation is weak/mixed on this short synthetic tape (off-diagonals ≈ −0.29…+0.15), so the off-diagonal cells read as pale blues and faint reds around a near-white center, with a saturated-red +1 diagonal. The *mechanism* — arbitrary per-cell colour via a rasterized colormap texture — is what a larger live correlation matrix would drive.
