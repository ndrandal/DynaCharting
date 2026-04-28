# Trial 223: Tetris Board

**Date:** 2026-03-22
**Goal:** 10x20 Tetris board with placed blocks in various colors and an active falling T-piece. Tests dense instancedRect@1 grid with multiple color groups.
**Outcome:** 28 placed blocks + 4 active blocks across 7 piece types. 31 unique IDs. Zero defects.

---

## What Was Built

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102 | 10 | Grid | lineAA@1 | 32 segs |
| various | 11 | Placed blocks | instancedRect@1 | 28 |
| active | 12 | Active T-piece | instancedRect@1 | 4 |

Board: 10 wide x 20 tall, cell size 0.08x0.08 clip units.

Total: 31 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- Blocks color-coded by piece type following standard Tetris guidelines.
- Active piece highlighted with brighter color on top layer.
- Grid lines provide cell boundaries.

### Done Wrong

Nothing.
