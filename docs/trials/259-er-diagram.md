# Trial 259: ER Diagram

**Date:** 2026-03-22
**Goal:** Entity-Relationship diagram with 4 entities (Customer, Order, Product, Category), 3 relationship diamonds, and 6 connection lines.
**Outcome:** 4 entity boxes, 3 diamonds (12 triangles), 6 connection lines. 13 unique IDs. Zero defects.

---

## What Was Built
Viewport 700x600. Dark background. 4 entity boxes (blue, instancedRect@1, cornerRadius=4).
3 relationship diamonds (orange, triSolid@1): places, contains, belongs_to.
Lines connect entities through relationship diamonds in standard ER notation.
Layout: Customer-Order top row, Product-Category bottom row.
Total: 13 unique IDs (1 pane, 3 layers, 3 buffers, 3 geometries, 3 drawItems).

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
- **Diamonds centered between entity pairs.** Correct ER diagram convention.
- **Connection lines terminate at diamond edges.** Line endpoints at diamond_size offset from center.
- **Rectangular layout avoids crossing lines.** 4 entities at corners, relationships on edges.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Diamond shapes from 4 triangles sharing center vertex.** Compact representation for ER relationships.
2. **Connection lines start/end at entity edges, not centers.** Offset by node_w/2 or node_h/2.
