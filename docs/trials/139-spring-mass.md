# Trial 139: Spring-Mass System

**Date:** 2026-03-22
**Goal:** Zigzag spring (lineAA@1, 10 zigzags) attached to wall (instancedRect@1) and mass block (instancedRect@1). Dashed equilibrium line.
**Outcome:** Spring-mass system diagram. Zero defects.

---

## What Was Built
Viewport 800x400. Spring-mass system diagram. Wall on left (instancedRect@1 with hatching), 10-zigzag spring (lineAA@1, 11 segments), mass block on right (instancedRect@1 with corner radius). Dashed vertical equilibrium line and horizontal ground line. All in clip space.

| DrawItem | Element | Pipeline | Color |
|---|---|---|---|
| wall | Wall | instancedRect@1 | gray |
| hatching | Wall hatching | lineAA@1 | dim |
| spring | Zigzag spring | lineAA@1 | cyan |
| mass | Mass block | instancedRect@1 | blue |
| equilibrium | Eq. line | lineAA@1 | dim dashed |
| ground | Ground | lineAA@1 | gray |

Total: 21 unique IDs (1 pane, 2 layers, 6 buffers, 6 geometrys, 6 drawItems).

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
- **Spring has 10 equal zigzag segments alternating above and below the center line.** 
- **Wall hatching uses diagonal lines on the fixed end, standard engineering convention for immovable supports.** 
- **Spring connects wall to mass block with endpoints at correct y=0 (center line).** 
- **Dashed equilibrium line passes through the mass center for reference.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Zigzag springs alternate y-offset at evenly spaced x positions along the spring length.** 
2. **Wall hatching convention: short diagonal lines on the fixed side indicate a rigid wall.** 
