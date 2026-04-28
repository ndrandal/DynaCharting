# Trial 267: Chess Opening Tree

**Date:** 2026-03-22
**Goal:** Move tree with 16 nodes across 4 levels, 15 edges. White-move nodes in cream, black-move nodes in dark. Rounded corners on all nodes.
**Outcome:** 16 nodes at correct positions, 15 edges connecting parents to children. 12 unique IDs. Zero defects.

---

## What Was Built

Viewport 800x600. Single pane with dark background.

**3 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 11 | White-move nodes | instancedRect@1 | 6 rects | cream |
| 105 | 11 | Black-move nodes | instancedRect@1 | 10 rects | dark gray |
| 108 | 10 | Edges | lineAA@1 | 15 segs | gray |

Tree structure: 1 root -> 2 (level 1) -> 5 (level 2) -> 8 (level 3). Edges connect parent bottom to child top.

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
- **Tree fans out naturally.** Root centered at top, wider spread at each level.
- **Edge endpoints connect to node edges.** Lines go from parent bottom (py - nh) to child top (cy + nh), not center-to-center.
- **White/black color coding.** Even-depth nodes (root, level 2) are light; odd-depth (level 1, 3) are dark.
- **cornerRadius gives professional look.** Rounded rectangles read as UI nodes.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Edge endpoints at node borders.** Subtract/add node half-height to connect lines to the edge, not the center.
2. **Depth-based coloring.** Alternating light/dark by tree depth creates clear level distinction.
