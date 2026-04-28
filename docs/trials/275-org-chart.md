# Trial 275: Org Chart

**Date:** 2026-03-22
**Goal:** 4-level organizational hierarchy: 1 CEO -> 3 VPs -> 6 directors -> 12 managers. Nodes (rounded rects) color-coded by level, connected by lines.
**Outcome:** 22 nodes across 4 levels, 21 connecting edges. 18 unique IDs. Zero defects.

---

## What Was Built

Viewport 900x500. Single pane with dark background.

**5 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 11 | CEO | instancedRect@1 | 1 rect | gold |
| 105 | 11 | VPs | instancedRect@1 | 3 rects | blue |
| 108 | 11 | Directors | instancedRect@1 | 6 rects | green |
| 111 | 11 | Managers | instancedRect@1 | 12 rects | gray |
| 114 | 10 | Edges | lineAA@1 | 21 segs | gray |

Hierarchy: 1 -> 3 -> 6 -> 12. Each parent connects to 2-3 children. Edges connect node bottom to child top.

Total: 18 unique IDs (1 pane, 2 layers, 5 buffers, 5 geometries, 5 drawItems).

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
- **Nodes spread evenly per level.** Each level distributed across the full width, wider levels below narrower ones.
- **Edges connect to node borders.** Parent bottom-edge to child top-edge prevents lines crossing through nodes.
- **Color hierarchy intuitive.** Gold CEO at top stands out; progressively cooler colors for lower ranks.
- **Binary branching creates balanced tree.** CEO -> 3, each VP -> 2, each director -> 2 managers.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **spread() helper for level layout.** Evenly distribute N nodes across a horizontal range at a given Y.
2. **Level-based coloring.** Distinct color per hierarchy level makes the structure immediately readable.
