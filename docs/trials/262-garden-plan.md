# Trial 262: Garden Plan

**Date:** 2026-03-22
**Goal:** Garden bed layout with raised beds (green rects), paths (brown rects), water feature (blue circle), dashed fence perimeter, decorative shrubs, and stepping stones.
**Outcome:** 4 beds, 5 paths, 1 water feature (24-seg circle), fence with dash pattern, 8 shrub points, 5 stepping stones. 22 unique IDs. Zero defects.

---

## What Was Built

Viewport 800x600. Single pane with earthy dark-green background (#1f2e14).

**6 DrawItems across 3 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Raised beds | instancedRect@1 | 4 rects | green |
| 105 | 10 | Paths | instancedRect@1 | 5 rects | brown |
| 108 | 11 | Water feature | triSolid@1 | 72 vtx (24 tris) | blue |
| 111 | 12 | Fence | lineAA@1 | 4 segs | tan, dashed |
| 114 | 11 | Shrubs | points@1 | 8 pts | bright green |
| 117 | 10 | Stepping stones | instancedRect@1 | 5 rects | gray, rounded |

Direct clip-space coordinates. No transform needed.

Total: 22 unique IDs (1 pane, 3 layers, 6 buffers, 6 geometries, 6 drawItems).

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
- **Beds and paths form a coherent garden grid.** Paths separate beds, creating walkable corridors.
- **Water feature uses 24-segment circle.** Placed in upper-right, visually distinct in blue.
- **Dashed fence creates perimeter boundary.** dashLength=0.04 with gapLength=0.02 for picket-fence look.
- **Shrubs along top path add decorative detail.** Evenly spaced points with larger pointSize.
- **Stepping stones have cornerRadius for natural look.** Rounded small rectangles in bottom area.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Earthy backgrounds suit garden themes.** Dark green (#1f2e14) reads as soil/turf under the layout.
2. **Dashed lineAA@1 for fence patterns.** dashLength and gapLength create convincing perimeter fencing.
