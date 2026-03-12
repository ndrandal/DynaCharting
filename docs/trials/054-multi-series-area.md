# Trial 054: Multi-Series Area Chart

**Date:** 2026-03-12
**Goal:** 4 overlapping filled area curves showing monthly revenue for 4 product lines, with line overlays on each curve. Tests triangle-strip tessellation between baseline and data curve (triSolid@1, 66 vertices each), back-to-front layering for overlapping semi-transparent fills, and paired fill+line rendering across 9 layers.
**Outcome:** All 4 area fills match expected data values at all 12 months. All 44 line segments correct. Layering produces correct overlap (largest series in back). Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**4 area fills (4 triSolid@1 DrawItems, pos2_clip, 66 vertices = 22 triangles each):**
Triangle strips between Y=0 baseline and data curve. 11 quads per series (12 data points).

| Series | Color | Fill Alpha | Layer | Peak Month | Peak Value |
|--------|-------|------------|-------|------------|------------|
| A (back) | #3b82f6 (blue) | 0.4 | 11 | Jun | 60 |
| B | #10b981 (emerald) | 0.4 | 12 | Jun | 40 |
| C | #f59e0b (amber) | 0.4 | 13 | Jul | 30 |
| D (front) | #8b5cf6 (violet) | 0.4 | 14 | Jul | 25 |

**4 line overlays (4 lineAA@1 DrawItems, rect4, 11 segments each):**
Same colors at alpha 0.8, lineWidth 1.5. Layers 15–18 (above all fills).

**5 grid lines (1 lineAA@1 DrawItem, rect4, 5 instances):**
At Y=10, 20, 30, 40, 50. Spanning X=[0, 13]. White, alpha 0.06, lineWidth 1. Layer 10.

Data space: X=[0, 13], Y=[0, 70]. Transform 50: sx=0.146154, sy=0.027143, tx=−0.95, ty=−0.95.

Layers: Grid (10) → Fills A–D (11–14) → Lines A–D (15–18).

Total: 38 unique IDs.

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

- **All 4 area fills match expected data.** Every curve vertex at months 1–12 verified against the spec's data table. All 48 data points (4 series × 12 months) are exact.

- **All 44 line segments correct.** Each of the 4 series has 11 segments connecting consecutive (month, value) pairs. All endpoints verified: 44/44 correct.

- **Back-to-front layering for overlapping areas.** Series A (largest, blue) on layer 11 (back), through to D (smallest, violet) on layer 14 (front). This ensures smaller series are visible in front of larger ones.

- **Semi-transparent fills create depth.** At alpha 0.4, overlapping regions show color mixing. The blue fill partially shows through the emerald, amber, and violet fills, creating natural depth perception.

- **Line overlays on separate layers above all fills.** Lines on layers 15–18 draw over all fill layers, ensuring crisp curve boundaries are never occluded by fills from other series.

- **Triangle strip tessellation is correct.** 11 quads (22 triangles, 66 vertices) per series. Each quad connects (x_i, 0), (x_i, y_i), (x_{i+1}, 0), (x_{i+1}, y_{i+1}).

- **All series share the seasonal pattern.** Revenue rises from January, peaks around June–July, then declines toward December. The pattern is clearly visible in the PNG as a mountain shape.

- **Transform math is exact.** sx=1.9/13=0.146154 and sy=1.9/70=0.027143 correctly map the data space to clip[−0.95, 0.95].

- **Grid lines at correct intervals.** Y=10, 20, 30, 40, 50 provide reference for reading values.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 9/9 geometries verified.

- **All 38 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Overlapping area charts need back-to-front layering.** The largest series goes furthest back so smaller series draw over it. Without this, the largest area would occlude all others.

2. **Area fills and line overlays need separate layer groups.** Fills on layers 11–14, lines on layers 15–18. This ensures all lines draw above all fills, so no line is hidden behind another series' fill.

3. **Triangle strips between baseline and curve create filled areas.** For each adjacent pair of data points, two triangles form a quad connecting the baseline (Y=0) to the curve. This produces a clean filled region with no gaps.

4. **Alpha 0.4 fills with alpha 0.8 lines create readable overlapping areas.** The fills are transparent enough to see through, while the lines are opaque enough to define each series clearly.

5. **9 layers for 4 series is the minimum for correct rendering.** 1 grid + 4 fills + 4 lines = 9 layers. Combining fills and lines on the same layer would cause z-fighting issues.
