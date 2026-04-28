# Trial 261: Room Furniture

**Date:** 2026-03-22
**Goal:** Top-down bedroom view with walls, door arc, window, bed, desk, bookshelf, nightstand, rug, and chair (circle).
**Outcome:** 15 wall/window/door segments. 4 furniture rects, 1 rug, 1 chair circle. 17 unique IDs. Zero defects.

---

## What Was Built
Viewport 700x700. Warm brown/wood-tone background (floor).
Walls (lineAA@1, lineWidth=3) form room boundary. Door arc in bottom-right corner.
Window on top wall (double line + muntin). Bed (large rect, top-left). Desk (right wall).
Bookshelf (left wall, narrow). Nightstand (next to bed). Rug (center, semi-transparent).
Chair near desk (10-segment circle, triSolid@1).
Total: 17 unique IDs (1 pane, 4 layers, 4 buffers, 4 geometries, 4 drawItems).

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
- **Furniture placed along walls realistically.** Bed in corner, desk against wall, bookshelf along wall.
- **Door arc shows swing direction.** Quarter circle opening into room.
- **Layer order: floor(rug) -> furniture -> walls -> chair.** Correct Z-ordering.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Interior design layouts: walls as lineAA@1, furniture as instancedRect@1.** Same approach as floor plan.
2. **Warm color palette (browns/tans) gives wood-floor feel.** Background color sets the mood.
