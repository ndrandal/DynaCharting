# Trial 245: Apartment Floor Plan

**Date:** 2026-03-22
**Goal:** Top-down apartment floor plan with 6 rooms as filled rectangles, thick walls as lineAA@1, and quarter-circle door arcs.
**Outcome:** 6 rooms rendered, 13 wall segments, 4 door arcs (24 arc segments). 13 unique IDs. Zero defects.

---

## What Was Built
Viewport 700x700. Apartment with living room, bedroom, kitchen, bathroom, study, and hallway.
Rooms drawn as instancedRect@1 with semi-transparent fills. Walls as lineAA@1 (lineWidth=3). Door arcs as quarter circles.
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
- **Room rectangles fill correctly.** All 6 rooms have non-overlapping regions covering the apartment area.
- **Wall lines trace room boundaries.** Outer walls form closed rectangle, inner walls align with room edges.
- **Door arcs indicate swing direction.** Quarter-circle arcs at room entrances show door swing.
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Architectural layouts map well to instancedRect@1.** Each room is a single rect instance — efficient.
2. **Door arcs need small segment counts.** 6 segments per quarter-circle is sufficient for visual recognition.
