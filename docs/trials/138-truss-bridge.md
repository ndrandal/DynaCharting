# Trial 138: Truss Bridge

**Date:** 2026-03-22
**Goal:** Warren truss with 11 nodes (points@1), 19 members (lineAA@1), and support triangles at ends (triSolid@1).
**Outcome:** Warren truss bridge. 6 bottom chord nodes at y=0, 5 top chord nodes at y=2.0 (offset by half-span). 19 members: 5 bottom chord, 4 top chord, 10 diagonals. Zero defects.

---

## What Was Built
Viewport 900x400. Warren truss bridge. 6 bottom chord nodes at y=0, 5 top chord nodes at y=2.0 (offset by half-span). 19 members: 5 bottom chord, 4 top chord, 10 diagonals. Support triangles at both ends. Span = 10.0 units.

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 11 | Members | lineAA@1 | 19 seg | white |
| 105 | 12 | Nodes | points@1 | 11 pts | cyan |
| 108 | 10 | Supports | triSolid@1 | 6 vtx | gray |

Total: 14 unique IDs (1 pane, 3 layers, 1 transform, 3 buffers, 3 geometrys, 3 drawItems).

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
- **Warren truss has alternating diagonal members creating a zigzag pattern between chords.** 
- **11 nodes (6 bottom + 5 top) and 19 members (5+4+10) match standard Warren truss topology.** 
- **Top chord nodes are offset by half the panel width, creating isosceles triangles.** 
- **Support triangles at both ends indicate pin and roller supports per structural convention.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Warren trusses have top chord nodes at midpoints between bottom chord nodes.** 
2. **19 members = 5 bottom + 4 top + 10 diagonals (each bottom-top pair has 2 diagonals).** 
