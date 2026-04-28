# Trial 112: Sine Wave

**Date:** 2026-03-22
**Goal:** Single period of sin(x) from 0 to 2pi with 50 lineAA segments and dashed amplitude reference lines.
**Outcome:** Single sine period with 51 sample points producing 50 lineAA@1 segments. Zero defects.

---

## What Was Built
Viewport 800x500. Single sine period with 51 sample points producing 50 lineAA@1 segments. Three dashed reference lines at y=+1, y=0, and y=-1 mark amplitude bounds and zero crossing. Transform maps data range [0,2pi]x[-1.2,1.2] to clip space.

| DrawItem | Layer | Element | Pipeline | Segments | Color |
|---|---|---|---|---|---|
| 102 | 11 | Sine curve | lineAA@1 | 50 | cyan |
| 105 | 10 | +1 ref | lineAA@1 | 1 | dim gray |
| 108 | 10 | -1 ref | lineAA@1 | 1 | dim gray |
| 111 | 10 | zero ref | lineAA@1 | 1 | gray |

Total: 16 unique IDs (1 pane, 2 layers, 1 transform, 4 buffers, 4 geometrys, 4 drawItems).

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
- **All 50 sine segments follow sin(x) exactly at 51 equispaced sample points from 0 to 2pi.** 
- **Amplitude reference lines at y=+1 and y=-1 span the full x range, confirming the sine touches both bounds.** 
- **Zero-crossing line at y=0 intersects the sine at x=0, pi, and 2pi as expected.** 
- **Transform correctly maps [0,2pi] to clipX [-0.9,0.9] and [-1.2,1.2] to clipY [-0.8,0.8].** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **lineAA@1 connected curves need N-1 segments for N sample points. vertexCount = N-1.** 
2. **Dashed reference lines use dashLength/gapLength style fields on the DrawItem, not separate geometry.** 
