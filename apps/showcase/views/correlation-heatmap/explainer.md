---
title: Correlation Heatmap — texture escape hatch
referenceTool: correlation matrix
tier: composed
---

An N×N correlation heatmap — each cell coloured by the pairwise correlation of two symbols' return series on a diverging blue↔red scale (blue = anti-correlated, white ≈ uncorrelated, red = correlated; the diagonal is +1). Per-cell colour is the wall the instanced path hits — one draw item carries one uniform colour — so the colormap is **rasterized upstream into an RGBA8 texture** at manifest-build time and blitted onto a pane-covering quad by the `texturedQuad@1` pipeline (the **ViewTexture escape hatch**, ENC-532).

| | |
|---|---|
| **DATA** | 4×4 Pearson correlation of AAPL/MSFT/NVDA/TSLA per-bucket log returns |
| **PIPELINE** | `texturedQuad@1` (one quad instance, samples a colormap texture) |
| **WRITE MODE** | static `textures` (setTexturePixels) + a one-quad `uploads` |
| **COMPOSED VIA** | matrix → diverging colormap → 256×256 RGBA8 raster → texture blit |
| **TEXTURE** | `700` · 256×256 RGBA8 (4×4 matrix upscaled ×64, with cell gridlines) |
| **SOURCE** | precomputed from `data/market/*` by `.build-composed-static.mjs` |

**What's going on (the technique).** The instanced and triangle pipelines colour a whole draw item with one uniform colour — they can't give every grid cell its own colour from a JSON manifest. The escape hatch is to compute the per-cell colours upstream and ship them as pixels: `.build-composed-static.mjs` computes the correlation matrix, maps each cell through a diverging blue↔red colormap, nearest-neighbour-upscales it to a crisp 256×256 RGBA8 image (with thin cell gridlines), and base64-encodes it. The manifest carries that image in `textures[]`; `applyManifest` uploads it with `setTexturePixels`, and a single `texturedQuad@1` instance covering the pane samples it via `setDrawItemTexture`. The engine itself stays pure — it only blits a producer-rasterized texture; all the per-cell colour logic lives in the build step.

> **Correlation values** (diverging colormap): the 4-symbol correlation is weak/mixed on this short synthetic tape (off-diagonals ≈ −0.29…+0.15), so the off-diagonal cells read as pale blues and faint reds around a near-white center, with a saturated-red +1 diagonal. The *mechanism* — arbitrary per-cell colour via a rasterized colormap texture — is what a larger live correlation matrix would drive.
