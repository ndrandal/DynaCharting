# Trial 038: Ternary Plot

**Date:** 2026-03-12
**Goal:** Soil composition ternary (triangle) plot — 18 data points across 3 soil types (Sandy, Loamy, Clayey) plotted on an equilateral triangle where axes represent Sand%, Silt%, Clay% (summing to 100%). Tests barycentric-to-Cartesian coordinate conversion (x = silt + clay/2, y = clay × sin 60°), triangular grid tessellation with lines parallel to all three edges, and aspect-corrected triAA@1 circles in a non-standard coordinate system.
**Outcome:** All 18 data point positions match barycentric formula to zero error. All 16 circle rim vertices at exactly 5.000px at every angle. All 12 grid lines have correct direction (±√3 slopes). All points inside triangle. Zero defects.

---

## What Was Built

A 700×700 viewport (square) with a single pane (background #0f172a):

**Triangle outline (lineAA@1, rect4, 3 instances, alpha 0.5, lineWidth 2):**
Bottom edge (0,0)→(100,0), right edge (100,0)→(50,86.6025), left edge (50,86.6025)→(0,0).

**12 internal grid lines at 20/40/60/80% (3 lineAA@1 DrawItems, rect4, 4 instances each, alpha 0.12, lineWidth 1):**

| Grid Set | Direction | Lines |
|----------|-----------|-------|
| Parallel to bottom (grid-clay) | Horizontal-ish | (10,17.32)→(90,17.32), (20,34.64)→(80,34.64), (30,51.96)→(70,51.96), (40,69.28)→(60,69.28) |
| Parallel to left edge (grid-silt) | Slope +√3 | (20,0)→(60,69.28), (40,0)→(70,51.96), (60,0)→(80,34.64), (80,0)→(90,17.32) |
| Parallel to right edge (grid-sand) | Slope −√3 | (80,0)→(40,69.28), (60,0)→(30,51.96), (40,0)→(20,34.64), (20,0)→(10,17.32) |

**18 data points (3 triAA@1 DrawItems, pos2_alpha, 864 vertices each = 6 circles × 144):**

| Group | Color | Alpha | Points (Sand/Silt/Clay → x, y) |
|-------|-------|-------|--------------------------------|
| Sandy | #3b82f6 (blue) | 0.85 | (70/20/10→25.0,8.66), (75/15/10→20.0,8.66), (65/25/10→30.0,8.66), (80/10/10→15.0,8.66), (60/20/20→30.0,17.32), (68/12/20→22.0,17.32) |
| Loamy | #10b981 (emerald) | 0.85 | (40/35/25→47.5,21.65), (35/40/25→52.5,21.65), (45/30/25→42.5,21.65), (30/35/35→52.5,30.31), (40/25/35→42.5,30.31), (50/30/20→40.0,17.32) |
| Clayey | #f59e0b (amber) | 0.85 | (15/25/60→55.0,51.96), (10/30/60→60.0,51.96), (20/20/60→50.0,51.96), (10/20/70→55.0,60.62), (15/35/50→60.0,43.30), (25/25/50→50.0,43.30) |

Circle radius: 5.000px core, 2.000px AA fringe. Aspect-corrected: X_data_radius=0.90226, Y_data_radius=0.86466 (ratio 1.04348 = px_per_dy/px_per_dx).

Data space: X=[−10, 110], Y=[−15, 100]. Transform 50: sx=0.0158333, sy=0.0165217, tx=−0.7917, ty=−0.7022.

Layers: grid (10) → outline (11) → points (12).

Total: 26 unique IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.
2. **AA fringe is 2.0px instead of standard 2.5px.** The standard FRINGE_PIXELS is 2.5, but the agent used 2.0px. Visually imperceptible.

---

## Spatial Reasoning Analysis

### Done Right

- **All 18 data point positions match the barycentric formula to zero error.** Every circle center was independently computed from (sand, silt, clay) → (silt + clay/2, clay × 0.86603) and compared to the buffer data. All 18 have error 0.00000 on both axes.

- **All 16 circle rim vertices at exactly 5.000px.** Checked all 16 segments of the first Sandy circle — pixel radius is 5.000 at every angle (0°, ±21.7°, ±43.8°, ±66.6°, ±90°, ±113.4°, ±136.2°, ±158.3°, 180°). Aspect correction correctly applied with X_data_radius / Y_data_radius = 1.04348.

- **All 18 points are inside the triangle.** Barycentric containment test confirms all three barycentric coordinates are non-negative for every data point.

- **Triangle outline is exact.** Three line segments connect exactly (0,0), (100,0), and (50, 86.6025), forming a perfect equilateral triangle.

- **Grid lines parallel to bottom edge are horizontal.** All 4 lines have identical Y at start and end (17.32, 34.64, 51.96, 69.28), confirming they're parallel to the base.

- **Grid lines parallel to left edge have slope exactly +√3.** All 4 lines in buffer 106 have dy/dx = 1.73205 = √3, confirming they're parallel to the left edge.

- **Grid lines parallel to right edge have slope exactly −√3.** All 4 lines in buffer 109 have dy/dx = −1.73205, confirming they're parallel to the right edge.

- **Grid intersections form the classic ternary grid pattern.** The 12 lines create a regular triangular mesh inside the triangle, dividing it into 25 small equilateral triangles (5×5 grid at 20% intervals).

- **Data clustering matches soil type definitions.** Sandy soils (high sand%) cluster in the bottom-left near the Sand vertex. Loamy soils (balanced) occupy the center. Clayey soils (high clay%) cluster in the upper region near the Clay vertex. The spatial grouping is immediately readable.

- **Transform math is exact.** X=[−10,110]→clip[−0.95,0.95] and Y=[−15,100]→clip[−0.95,0.95]. All four transform components verified to machine precision.

- **Square viewport eliminates aspect distortion.** 700×700 with symmetric clip ranges. The aspect ratio (px_per_dy/px_per_dx = 1.04348) comes from the non-square data range (120 vs 115 units), correctly handled by the aspect-corrected circle radii.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All vertex counts match buffer data.** Outline: 12/4=3 ✓. Each grid set: 16/4=4 ✓. Each circle group: 2592/3=864=6×144 ✓.

- **All 26 IDs unique.** No collisions across the unified namespace.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Ternary plots use barycentric-to-Cartesian conversion.** Given (a%, b%, c%) where a+b+c=100, the standard ternary mapping places the three pure components at the triangle vertices. The conversion x = b + c/2, y = c × sin(60°) naturally produces an equilateral triangle in Cartesian space.

2. **Triangular grids require three sets of parallel lines.** Unlike rectangular grids (two sets), ternary grids need lines parallel to all three triangle edges. Each set has slope 0 (horizontal), +√3, or −√3 for an equilateral triangle.

3. **Non-square data ranges in square viewports require aspect correction.** Here X spans 120 units and Y spans 115 units in a 700×700 viewport, creating a 4.3% aspect distortion. Circle radii must be corrected per-axis to render as circles, not ellipses.

4. **Ternary data naturally clusters by composition.** High-A samples cluster near the A vertex, balanced samples near the center, and high-C samples near the C vertex. This spatial encoding is the fundamental value of the ternary plot — it makes composition relationships immediately visible.

5. **lineAA@1 with low alpha (0.12) creates effective reference grids without competing with data.** The same pattern works for triangular grids as it does for rectangular ones.
