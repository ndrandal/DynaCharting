# Trial 272: Color Wheel

**Date:** 2026-03-22
**Goal:** 12-segment color wheel with primary/secondary/tertiary colors, hollow center, and 6 complementary-pair connecting lines.
**Outcome:** 12 wedges at 30-degree intervals, inner circle cutout (R=0.35), 6 dashed complementary lines. 46 unique IDs. Zero defects.

---

## What Was Built

Viewport 600x600. Single pane with dark background.

**14 DrawItems across 3 layers:**

| DrawItem | Layer | Element | Pipeline | Count |
|----------|-------|---------|----------|-------|
| 102..135 | 10 | 12 color wedges | triSolid@1 | 48 tris total |
| 138 | 11 | Inner circle | triSolid@1 | 32 tris |
| 141 | 12 | Complementary lines | lineAA@1 | 6 segs |

Wheel outer R=0.7, inner R=0.35. Each wedge = 4 triangle fan sectors.

Total: 46 unique IDs (1 pane, 3 layers, 14 buffers, 14 geometries, 14 drawItems).

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
- **12 segments span full 360 degrees.** Each wedge covers exactly 30 degrees, starting from top (12 o'clock).
- **Color progression follows the spectrum.** Red -> Orange -> Yellow -> Green -> Blue -> Violet and back.
- **Inner circle creates donut shape.** Dark center circle on higher layer covers the wedge interiors.
- **Complementary pairs connected by dashed lines.** Lines through center connect opposite colors (e.g., red-green, blue-orange).
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Sector fans for wheel segments.** sector_fan with different start/end angles creates pie/wheel slices.
2. **Donut shape via layered circles.** Full circle on top of wedges with background color creates a hollow center.
