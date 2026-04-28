# Trial 212: Chessboard

**Date:** 2026-03-22
**Goal:** 8x8 chessboard with alternating black/white squares, border, and coordinate markers. Tests grid layout with instancedRect@1, lineAA@1 border, and points@1 markers.
**Outcome:** 64 squares (32 white + 32 dark) correctly tiled. Border and markers rendered. 15 unique IDs. Zero defects.

---

## What Was Built

A 600x600 viewport with a single pane (dark background):

**4 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | White squares | instancedRect@1 | 32 rects | cream |
| 105 | 10 | Dark squares | instancedRect@1 | 32 rects | brown |
| 108 | 11 | Border | lineAA@1 | 4 segs | tan |
| 111 | 11 | Coordinate markers | points@1 | 16 pts | gray |

Board spans [-0.8, 0.8] in clip space. Each square is 0.2x0.2. No transform needed (direct clip coords).

Total: 15 unique IDs.

---

## Defects Found

None.

---

## Spatial Reasoning Analysis

### Done Right

- 64 squares perfectly tile the 8x8 grid with no gaps or overlaps.
- Alternating pattern correct: (row+col)%2 determines color.
- Border outlines the full board edge.
- Coordinate markers placed outside the board edge.

### Done Wrong

Nothing.
