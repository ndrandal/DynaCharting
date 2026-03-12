# Trial 045: Box-and-Whisker Plot

**Date:** 2026-03-12
**Goal:** 8 statistical distributions displayed as box-and-whisker plots with 5 component types: filled boxes (instancedRect@1), median lines, vertical whiskers, horizontal caps (all lineAA@1), and outlier dots (triAA@1 aspect-corrected circles). Tests precise multi-element vertical alignment, grouped line segment rendering, and aspect-corrected outlier circles at extreme aspect ratio (18.5:1 px_per_dx:px_per_dy).
**Outcome:** All 8 boxes, 16 whiskers, 16 caps, 8 medians, and 10 outlier circles match the spec exactly. Outlier circles are perfectly circular at 8px radius despite 18.5:1 aspect ratio. All 67 IDs unique. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**8 filled boxes (8 instancedRect@1 DrawItems, rect4, 1 instance each):**

| Cat | X Center | Q1 | Q3 | Box [xMin,yMin,xMax,yMax] | Color | Alpha |
|-----|----------|-----|-----|--------------------------|-------|-------|
| A | 1 | 30 | 55 | [0.75, 30, 1.25, 55] | #3b82f6 (blue) | 0.7 |
| B | 2 | 40 | 62 | [1.75, 40, 2.25, 62] | #10b981 (emerald) | 0.7 |
| C | 3 | 22 | 48 | [2.75, 22, 3.25, 48] | #f59e0b (amber) | 0.7 |
| D | 4 | 48 | 70 | [3.75, 48, 4.25, 70] | #ec4899 (pink) | 0.7 |
| E | 5 | 35 | 58 | [4.75, 35, 5.25, 58] | #8b5cf6 (violet) | 0.7 |
| F | 6 | 45 | 65 | [5.75, 45, 6.25, 65] | #06b6d4 (cyan) | 0.7 |
| G | 7 | 18 | 40 | [6.75, 18, 7.25, 40] | #f97316 (orange) | 0.7 |
| H | 8 | 52 | 75 | [7.75, 52, 8.25, 75] | #ef4444 (red) | 0.7 |

Box width = 0.5 data units (±0.25 from center X).

**16 whisker lines (1 lineAA@1 DrawItem, rect4, 16 instances):**
White, alpha 0.6, lineWidth 1.5. Two vertical segments per box: lower whisker (whiskerLo → Q1) and upper whisker (Q3 → whiskerHi). All centered at category X.

**16 cap lines (1 lineAA@1 DrawItem, rect4, 16 instances):**
White, alpha 0.6, lineWidth 1.5. Two horizontal segments per box at whisker endpoints. Cap width = 0.3 data units (±0.15 from center X).

**8 median lines (1 lineAA@1 DrawItem, rect4, 8 instances):**
White, alpha 0.6, lineWidth 1.5. One horizontal segment per box at median Y, spanning box width [cx−0.25, cx+0.25].

**10 outlier circles (8 triAA@1 DrawItems, pos2_alpha, 48 verts per circle):**
Aspect-corrected: rx=0.0758 data units, ry=1.404 data units → 8px circular in pixel space. 16 segments per circle, center-fan tessellation. Alpha 1.0 at center, 0.0 at rim. Same color as parent box at alpha 0.9.

| Cat | Outliers | Circle Count |
|-----|----------|-------------|
| A | 5, 82 | 2 (96 verts) |
| B | 12 | 1 (48 verts) |
| C | 80 | 1 (48 verts) |
| D | 95 | 1 (48 verts) |
| E | 8 | 1 (48 verts) |
| F | 18, 90 | 2 (96 verts) |
| G | 68 | 1 (48 verts) |
| H | 28 | 1 (48 verts) |

**6 grid lines (1 lineAA@1 DrawItem, rect4, 6 instances):**
At Y=0, 20, 40, 60, 80, 100. Spanning X=[0, 9]. White, alpha 0.06, lineWidth 1.

Data space: X=[0, 9], Y=[0, 100]. Transform 50: sx=0.211111, sy=0.019, tx=−0.95, ty=−0.95.

Layers: Grid (10) → Boxes (11) → Whiskers/Caps (12) → Medians (13) → Outliers (14).

Total: 67 unique IDs.

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

- **All 8 boxes at correct Q1–Q3 ranges.** Every box's yMin matches Q1 and yMax matches Q3. Box width is ±0.25 from category center X. All 8/8 verified.

- **All 16 whiskers at correct positions.** Lower whiskers connect whiskerLo to Q1, upper whiskers connect Q3 to whiskerHi. All 16 segments centered at their category X coordinate. 16/16 verified.

- **All 16 caps at correct whisker endpoints.** Each cap is a horizontal line at the whisker-low or whisker-high Y value, spanning ±0.15 data units from center X. 16/16 verified.

- **All 8 medians at correct Y positions.** Each median line spans the full box width at the exact median Y value. 8/8 verified.

- **All 10 outlier circles at correct positions.** Each outlier dot is centered at (category X, outlier Y). Categories A and F have 2 outliers each; the rest have 1. All 10 positions verified against spec.

- **Outlier circles are perfectly aspect-corrected.** With px_per_dx=105.56 and px_per_dy=5.70 (ratio 18.5:1), the X radius is 0.0758 data units and Y radius is 1.404 data units, both producing exactly 8.0 pixels. The pixel-space radius spread across all 16 rim vertices is ≤0.0001px. Circles appear perfectly circular despite the extreme aspect ratio.

- **Transform math is exact.** sx=1.9/9=0.211111 maps X=[0,9] to clip[−0.95,0.95]. sy=1.9/100=0.019 maps Y=[0,100] to clip[−0.95,0.95]. Verified.

- **Layer ordering produces correct visual stacking.** Grid (10, back) → Boxes (11) → Whiskers/Caps (12) → Medians (13) → Outliers (14, front). Medians draw over box fills, whiskers draw over box edges, outliers draw over everything.

- **Grid lines at correct 20-unit intervals.** 6 horizontal lines at Y=0,20,40,60,80,100, spanning the full X range [0,9].

- **Grouped line segments minimize DrawItem count.** All 16 whiskers share one DrawItem, all 16 caps share one, all 8 medians share one. Only the boxes and outliers need per-category DrawItems (for different colors). Total: 20 DrawItems for a 5-component × 8-category chart.

- **Outlier colors match their parent boxes.** Each outlier DrawItem uses the same RGB as its corresponding box DrawItem, at alpha 0.9 (vs box alpha 0.7).

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓.

- **All buffer sizes match vertex counts.** All 20 geometries verified: buffer float count = vertexCount × floats-per-vertex. 20/20 correct.

- **All 67 IDs unique.** No collisions across buffers (100–119), geometries (200–219), drawItems (300–319), transform (50), pane (1), layers (10–14).

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Box-and-whisker plots have 5 distinct component types.** Boxes (instancedRect@1), whiskers (vertical lineAA@1), caps (horizontal lineAA@1), medians (horizontal lineAA@1), and outliers (triAA@1 circles). Each component type has different visual properties but must align precisely at shared coordinates.

2. **Grouping same-styled line segments into one DrawItem is highly efficient.** 16 whiskers, 16 caps, and 8 medians require only 3 DrawItems total (vs 40 if each were separate). This works because all whiskers share the same color/alpha/lineWidth, and similarly for caps and medians.

3. **Extreme aspect ratios demand careful aspect correction.** With X=[0,9] and Y=[0,100] in a 1000×600 viewport, px_per_dx:px_per_dy is 18.5:1. Without correction, circles would be 18.5× taller than wide. The agent correctly computed separate X and Y data-space radii to produce 8px pixel-space circles.

4. **Outlier circles need per-category DrawItems for color.** Unlike whiskers/caps/medians (all white), outliers match their box color. This forces one DrawItem per category for outliers, but the count is bounded by the number of categories, not outlier count.

5. **Box width and cap width should use different sizes for visual hierarchy.** Boxes at ±0.25 (0.5 data units) and caps at ±0.15 (0.3 data units) create clear visual distinction — the box is the primary element, while caps serve as secondary reference marks.
