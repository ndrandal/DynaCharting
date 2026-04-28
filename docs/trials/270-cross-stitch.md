# Trial 270: Cross Stitch

**Date:** 2026-03-22
**Goal:** 20x20 grid (400 rects) with a heart shape using the implicit heart equation. Background fabric, red fill interior, dark red border stitches.
**Outcome:** 222 background + 138 interior + 40 border = 400 total stitches. Heart shape via (x^2+y^2-1)^3 - x^2*y^3 < 0. 11 unique IDs. Zero defects.

---

## What Was Built

Viewport 600x600. Single pane with dark warm background.

**3 DrawItems across 1 layer:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Background fabric | instancedRect@1 | 222 rects | off-white |
| 105 | 10 | Heart interior | instancedRect@1 | 138 rects | red |
| 108 | 10 | Heart border | instancedRect@1 | 40 rects | dark red |

Cell size: 0.0800 x 0.0800. Heart centered at (9.5, 9.5) in grid coordinates.

Total: 11 unique IDs (1 pane, 1 layer, 3 buffers, 3 geometries, 3 drawItems).

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
- **Implicit heart equation produces recognizable shape.** The algebraic curve (x^2+y^2-1)^3 - x^2*y^3 < 0 gives a classic heart outline.
- **Border detection via neighbor check.** Cells adjacent to non-heart cells form the darker outline ring.
- **Three distinct colors.** Off-white background, bright red fill, dark red outline creates depth.
- **All 400 cells accounted for.** 222 + 138 + 40 = 400 = 400.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Implicit equations for shapes.** f(x,y) < 0 inside the shape is a powerful way to classify pixels/cells.
2. **Border detection via neighbor adjacency.** Check 4 neighbors; if any is outside, the cell is on the border.
