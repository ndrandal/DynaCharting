# Trial 250: Basketball Play

**Date:** 2026-03-22
**Goal:** Half-court diagram with court markings, 3-point arc, key area, 5 player dots, and 3 movement arrow paths.
**Outcome:** Court with 49 line segments, 5 players, 3 arrow paths with arrowheads. 16 unique IDs. Zero defects.

---

## What Was Built
Viewport 600x700. Brown court background. Gold court lines.
Court markings: baseline, sidelines, half-court line, key (paint), free-throw circle arc, 3-point arc + straights, basket circle.
5 blue player dots at positions. 3 dashed yellow arrow paths showing play movement with triangle arrowheads.
Total: 16 unique IDs (1 pane, 3 layers, 4 buffers, 4 geometries, 4 drawItems).

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
- **3-point arc sweeps correctly from wing to wing.** 20-segment arc at R=0.6 from baseline.
- **Key area proportions realistic.** Width and height approximate NBA lane dimensions.
- **Arrow paths use dashed lines.** Distinguishable from solid court lines.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Sports court diagrams combine arcs, lines, and dots.** All achievable with lineAA@1 and triSolid@1.
2. **Arrow heads are simple triangles oriented with atan2.** The arrow_head helper computes the triangle from direction angle.
