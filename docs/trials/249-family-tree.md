# Trial 249: Family Tree

**Date:** 2026-03-22
**Goal:** 3-generation family tree with 7 person nodes (rounded rects) connected by hierarchical lines.
**Outcome:** 7 nodes across 3 generations, 12 connection segments. 9 unique IDs. Zero defects.

---

## What Was Built
Viewport 600x700. 3-generation family tree.
Gen 1 (y=0.6): 2 grandparents. Gen 2 (y=0.0): 3 children (uncle, parent, aunt). Gen 3 (y=-0.6): 2 grandchildren from parent.
Nodes are instancedRect@1 with cornerRadius=6. Connectors are lineAA@1 with orthogonal routing.
Total: 9 unique IDs (1 pane, 2 layers, 2 buffers, 2 geometries, 2 drawItems).

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
- **3 generation levels at y=0.6, 0.0, -0.6.** Clear visual hierarchy top-to-bottom.
- **Orthogonal connectors route through intermediate horizontal lines.** Standard family tree layout.
- **Couple indicated by horizontal line between grandparents.** Visual convention.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Tree layouts use horizontal+vertical line routing.** No diagonal connectors needed for hierarchy diagrams.
2. **cornerRadius on instancedRect@1 makes nodes look like proper UI cards.**
