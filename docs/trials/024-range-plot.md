# Trial 024: Range Plot

**Date:** 2026-03-12
**Goal:** Fourteen-day temperature range plot with vertical low–high bars (instancedRect@1), a connected mean line (lineAA@1), and mean-value dots (triAA@1). First trial combining all three pipeline types in a time-series layout, testing 4-layer depth ordering and extreme aspect correction (0.14:1).
**Outcome:** All 14 range bars, 13 line segments, and 14 dot centers are exact. Circles perfectly circular at 5px despite 0.14:1 aspect ratio. Transform, text labels, and grid lines all verified. Zero defects.

---

## What Was Built

A 1100×550 viewport with a single pane (980×460px, 80px left/40px right/50px top/40px bottom margins):

**14 range bars (1 instancedRect@1 DrawItem, rect4, 14 instances):**

| Day | Low | High | Range | Bar rect4 |
|-----|-----|------|-------|-----------|
| 1 | 42 | 58 | 16 | [0.8, 42, 1.2, 58] |
| 2 | 45 | 62 | 17 | [1.8, 45, 2.2, 62] |
| 3 | 48 | 67 | 19 | [2.8, 48, 3.2, 67] |
| 4 | 52 | 71 | 19 | [3.8, 52, 4.2, 71] |
| 5 | 50 | 68 | 18 | [4.8, 50, 5.2, 68] |
| 6 | 44 | 60 | 16 | [5.8, 44, 6.2, 60] |
| 7 | 40 | 55 | 15 | [6.8, 40, 7.2, 55] |
| 8 | 38 | 52 | 14 | [7.8, 38, 8.2, 52] |
| 9 | 41 | 56 | 15 | [8.8, 41, 9.2, 56] |
| 10 | 46 | 64 | 18 | [9.8, 46, 10.2, 64] |
| 11 | 50 | 70 | 20 | [10.8, 50, 11.2, 70] |
| 12 | 54 | 74 | 20 | [11.8, 54, 12.2, 74] |
| 13 | 51 | 69 | 18 | [12.8, 51, 13.2, 69] |
| 14 | 47 | 63 | 16 | [13.8, 47, 14.2, 63] |

Light blue (#60a5fa), alpha 0.5, cornerRadius 2. Bar half-width 0.2 data units.

**13 mean-line segments (1 lineAA@1 DrawItem, rect4, 13 instances):**
Each connects consecutive days' mean temperatures. Orange (#f59e0b), alpha 0.9, lineWidth 2.

**14 mean dots (1 triAA@1 DrawItem, pos2_alpha, 2016 vertices):**
Orange (#f59e0b), alpha 1.0. 14 circles × 144 vertices each. Means: 50, 53.5, 57.5, 61.5, 59, 52, 47.5, 45, 48.5, 55, 60, 64, 60, 55.

Circle radii: X=0.076531, Y=0.543478 data units. Pixel radius: 5.000px both axes. Aspect correction: 0.1408 (px_per_dy/px_per_dx = 9.2/65.33). Fringe: 2.5px.

**6 horizontal grid lines (1 lineAA@1 DrawItem, rect4, 6 instances):**
At Y=30,40,50,60,70,80. White, alpha 0.07, lineWidth 1.

Data space: X=[0, 15], Y=[30, 80]. Transform: sx=0.118788, sy=0.033455, tx=−0.854545, ty=−1.858182.

Layers: grid (10) → bars (11) → line (12) → dots (13).

Total: 1 pane, 4 layers, 1 transform, 4 buffers, 4 geometries, 4 drawItems = 18 IDs.

---

## Defects Found

### Critical

None.

### Major

None.

### Minor

1. **Text labels invisible in PNG capture.** Known limitation.

---

## Spatial Reasoning Analysis

### Done Right

- **All 14 range bars are exact.** Every bar matches [day±0.2, low, high] from the data table. All 14 verified — zero position errors.

- **All 13 mean-line segments connect correct values.** Segment 0: (1,50)→(2,53.5), ..., Segment 12: (13,60)→(14,55). The line correctly traces the mean temperature wave pattern.

- **All 14 mean dot centers are exact.** Day 1 at (1,50), Day 2 at (2,53.5), ..., Day 14 at (14,55). All match computed means (low+high)/2.

- **Dots are perfectly circular at 5px.** X radius 0.076531 × 65.333 px/unit = 5.000px. Y radius 0.543478 × 9.200 px/unit = 5.000px. The 0.14:1 aspect correction (most extreme inverse ratio in any trial — X is ~7× smaller than Y in data units) is handled correctly.

- **Fringe exactly 2.5px.** X fringe: 0.038265 × 65.333 = 2.500px.

- **Transform is exact.** X=0→clipX=−0.855, X=15→clipX=0.927, Y=30→clipY=−0.855, Y=80→clipY=0.818. All verified.

- **4-layer ordering creates correct visual hierarchy.** Grid (behind) → range bars → mean line → dots (on top). The orange dots sit on top of the orange line, which passes through the blue bars.

- **All vertex formats correct.** lineAA@1 uses rect4 ✓ (grid and mean line), instancedRect@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All vertex counts match.** Grid: 24/4=6 ✓. Bars: 56/4=14 ✓. Line: 52/4=13 ✓. Dots: 6048/3=2016=14×144 ✓.

- **Text label positions match transform.** Y-axis: 30°F→clipY=−0.855, 80°F→clipY=0.818. X-axis: Day 1→clipX=−0.736, Day 14→clipX=0.808. All ≤0.001 of expected.

- **Temperature wave pattern is visually clear.** Rise days 1–4, dip 5–8, rise 9–12, dip 13–14. The mean line and bar heights both encode this, creating redundant visual confirmation.

- **All 18 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Range plots combine three pipeline types naturally.** instancedRect@1 for range bars, lineAA@1 for the connected trend line, triAA@1 for emphasis dots. All share one transform and one data coordinate space.

2. **Extreme inverse aspect correction works.** With px_per_dy (9.2) much smaller than px_per_dx (65.33), the Y data radius must be ~7× the X data radius. This is the opposite extreme from Trial 020/023 where px_per_dy was larger. The tessellation handles both directions.

3. **Thin vertical bars (0.4 data units) are effective.** At 0.4 × 65.33 = 26px width, the bars are wide enough to be clearly visible but narrow enough that the mean line and dots are readable without occlusion.

4. **Mean line segments connect day-to-day, not min-to-max.** Unlike connector lines in dumbbell charts, the mean line connects consecutive time points, creating a polyline. With lineAA@1, this requires N−1 instances for N data points.

5. **The cornerRadius on range bars adds polish.** At 2px on tall narrow bars (~90–180px height), the rounded caps are subtle but give the bars a modern appearance.
