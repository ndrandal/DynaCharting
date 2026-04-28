# Trial 220: Pixel Art Mushroom

**Date:** 2026-03-22
**Goal:** 16x16 pixel art Mario-style mushroom. Red cap with white spots, tan stem, black outline. Each pixel = one instancedRect@1.
**Outcome:** 200 colored pixels rendered across 5 color groups. 17 unique IDs. Zero defects.

---

## What Was Built

| Color | Pixels | Description |
|-------|--------|-------------|
| Black | 60 | Outline |
| Red | 58 | Cap |
| White | 24 | Spots |
| Tan | 28 | Stem |
| Skin | 30 | Face |

Pixel size: 0.1 clip units. Grid: 16x16 = 256 cells, 200 filled.

Total: 17 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Mushroom shape recognizable with cap, spots, stem, and face.
- Pixel grid correctly flipped (row 0 at bottom).
- 5-color palette creates clear visual separation.

### Done Wrong

Nothing.
