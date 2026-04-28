# Trial 136: Phasor Diagram

**Date:** 2026-03-22
**Goal:** 3 phasors at 30deg, 150deg, 270deg with arrowheads (lineAA@1 + triSolid@1) on a dashed unit circle.
**Outcome:** Phasor diagram with 3 phasors at 30, 150, and 270 degrees with magnitudes 0.7, 0.5, 0.6 (scaled to unit circle R=0.8). Zero defects.

---

## What Was Built
Viewport 600x600. Phasor diagram with 3 phasors at 30, 150, and 270 degrees with magnitudes 0.7, 0.5, 0.6 (scaled to unit circle R=0.8). Each phasor is a lineAA@1 arrow from origin with a triSolid@1 arrowhead. Dashed unit circle reference. Point markers at tips. Axes through origin.

| Layer | Elements | Pipeline | Color |
|---|---|---|---|
| 10 | Unit circle + axes | lineAA@1 | dim |
| 11 | 3 phasors + 3 arrowheads | lineAA + triSolid | cyan/yellow/green |
| 12 | 3 tip points | points@1 | white |

Total: 31 unique IDs (1 pane, 3 layers, 9 buffers, 9 geometrys, 9 drawItems).

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
- **All 3 phasors originate from (0,0) and extend to correct polar coordinates.** 
- **Arrowheads point in the direction of each phasor, computed from the angle.** 
- **Dashed unit circle provides magnitude reference — phasor tip distance from center shows relative magnitude.** 
- **120-degree angular separation between phasors (30, 150, 270) resembles a 3-phase power system.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Phasor arrows use (0,0) → (m*cos(a), m*sin(a)) in clip space — no transform needed for a symmetric diagram.** 
2. **Arrowheads are small triangles oriented along the phasor direction using perpendicular offsets.** 
