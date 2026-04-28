# Trial 252: Emergency Exit Map

**Date:** 2026-03-22
**Goal:** Building floor plan with walls, 3 green exit doors, dashed escape route, and "you are here" marker.
**Outcome:** 8 wall segments, 3 exit doors, dashed route with arrowhead. 17 unique IDs. Zero defects.

---

## What Was Built
Viewport 800x600. Dark building interior. Gray walls (lineAA@1, lineWidth=3).
3 green exit doors (instancedRect@1). Dashed red escape route from marker to nearest exit.
Red "you are here" hexagon marker (triSolid@1 circle_fan). Route arrowhead at exit.
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
- **Route follows corridors.** Path turns at wall intersections, doesn't pass through walls.
- **Exit doors are visually distinct green.** Immediately recognizable on dark background.
- **Dashed route line distinguishes from solid walls.** Different visual language for wayfinding vs structure.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Wayfinding maps combine structure (walls) + annotation (route).** Layer separation keeps them independent.
2. **Dashed lines (dashLength/gapLength) convey "path to follow" vs solid "physical boundary".**
