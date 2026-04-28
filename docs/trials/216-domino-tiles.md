# Trial 216: Domino Tiles

**Date:** 2026-03-22
**Goal:** 6 domino tiles with pip dots. Tiles: 1|2, 3|4, 5|6, 6|6, 0|3, 2|5. Tests instancedRect@1 with cornerRadius, lineAA@1 dividers, triSolid@1 dot circles.
**Outcome:** 6 tiles with 43 total dots rendered correctly. 13 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Tile bodies | instancedRect@1 | 6 |
| 105 | 11 | Divider lines | lineAA@1 | 6 |
| 108 | 12 | Dots | triSolid@1 | 43 circles |

Each dot = 10-triangle fan. Total: 13 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Domino tiles correctly sized and spaced horizontally.
- Dot patterns follow standard pip layouts (1-6 and blank).
- Divider lines bisect each tile.
- Rounded corners on tile bodies.

### Done Wrong

Nothing.
