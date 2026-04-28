# Trial 265: Theater Seating

**Date:** 2026-03-22
**Goal:** 5 curved rows of theater seats arranged in arcs above a stage. Color-coded: available (green), taken (red), selected (yellow). Tests polar-to-cartesian arc layout.
**Outcome:** 80 seats in 5 arcs (10, 12, 16, 20, 22 per row), 1 stage rect. 49 available, 26 taken, 5 selected. 15 unique IDs. Zero defects.

---

## What Was Built

Viewport 800x600. Single pane with dark background.

**4 DrawItems across 2 layers:**

| DrawItem | Layer | Element | Pipeline | Count | Color |
|----------|-------|---------|----------|-------|-------|
| 102 | 10 | Stage | instancedRect@1 | 1 rect | brown, rounded |
| 105 | 11 | Available seats | instancedRect@1 | 49 rects | green |
| 108 | 11 | Taken seats | instancedRect@1 | 26 rects | red |
| 111 | 11 | Selected seats | instancedRect@1 | 5 rects | yellow |

Seats arranged on arcs from 30 to 150 degrees (centered on stage top edge). Row radii: 0.35, 0.48, 0.61, 0.74, 0.87.

Total: 15 unique IDs (1 pane, 2 layers, 4 buffers, 4 geometries, 4 drawItems).

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
- **Seats follow curved arcs.** Each row is a semicircular arc from 30 to 150 degrees, creating a natural amphitheater shape.
- **Row count increases with radius.** Inner rows have fewer seats (10), outer rows more (22), matching real theaters.
- **Three-state color coding is intuitive.** Green=available, red=taken, yellow=selected is universally understood.
- **Stage rect anchors the bottom.** Brown rounded rectangle provides visual reference for the front of house.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Arc layout = polar coordinates.** Distribute seats evenly across an angular range, converting (r, theta) to (x, y).
2. **Increasing seat count per row.** Outer rows need more seats to fill the longer arc; inner rows fewer.
