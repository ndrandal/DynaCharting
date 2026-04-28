# Trial 269: Knitting Pattern

**Date:** 2026-03-22
**Goal:** 12x16 stitch grid (192 rects) with two yarn colors forming a diamond motif using Manhattan distance.
**Outcome:** 152 background stitches (cream) + 40 motif stitches (dark red) = 192 total. Diamond centered at (5.5, 7.5) with radius 4.5. 12 unique IDs. Zero defects.

---

## What Was Built

Viewport 500x650. Single pane with dark warm background.

**3 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Background stitches | instancedRect@1 | 152 rects | cream |
| 105 | 10 | Diamond stitches | instancedRect@1 | 40 rects | dark red |
| 108 | 11 | Grid lines | lineAA@1 | 30 segs | brown, lw=0.5 |

Cell size: 0.1333 x 0.1000. Diamond uses Manhattan distance (|col-cx| + |row-cy| <= 4.5).

Total: 12 unique IDs (1 pane, 2 layers, 3 buffers, 3 geometries, 3 drawItems).

---

## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---

## Spatial Reasoning Analysis
### Done Right
- **Manhattan distance creates a perfect diamond.** The L1 norm produces a rotated-square (diamond) shape centered in the grid.
- **Warm yarn colors.** Cream (#d9bf99) and dark red (#8c2626) are classic knitting palette colors.
- **Thin grid lines suggest stitch boundaries.** lineWidth=0.5 with low alpha doesn't overpower the pattern.
- **Cell padding prevents visual merging.** 0.006 gap between stitches simulates yarn texture.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Manhattan distance for diamond shapes.** |dx| + |dy| <= r gives a clean rotated-square motif.
2. **Warm browns for textile backgrounds.** Earth tones suit craft patterns.
