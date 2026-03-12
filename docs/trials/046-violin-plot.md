# Trial 046: Violin Plot

**Date:** 2026-03-12
**Goal:** 6 violin plots with mirrored Gaussian density curves (triSolid@1, 174 vertices each), inner Q1–Q3 boxes (instancedRect@1), and aspect-corrected median dots (triAA@1). Tests symmetric triangle-strip tessellation, Gaussian width function, and multi-component vertical alignment at extreme data-space aspect ratio (X=[0,7], Y=[0,100]).
**Outcome:** All 6 violin shapes match the Gaussian width formula to ≤0.000001 error. Perfect bilateral symmetry (zero symmetry error). All inner boxes and median dots at correct positions. Median dots perfectly circular at 8px despite 23.8:1 aspect ratio. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**6 violin fills (6 triSolid@1 DrawItems, pos2_clip, 174 vertices = 58 triangles each):**

| Cat | Center X | Y Range | Peak Y | σ | Max Width | Color | Alpha |
|-----|----------|---------|--------|---|-----------|-------|-------|
| A | 1 | 10–80 | 45 | 15 | 0.35 | #3b82f6 (blue) | 0.7 |
| B | 2 | 15–90 | 55 | 18 | 0.30 | #10b981 (emerald) | 0.7 |
| C | 3 | 5–70 | 35 | 12 | 0.40 | #f59e0b (amber) | 0.7 |
| D | 4 | 20–85 | 50 | 14 | 0.32 | #ec4899 (pink) | 0.7 |
| E | 5 | 0–75 | 40 | 16 | 0.28 | #8b5cf6 (violet) | 0.7 |
| F | 6 | 25–95 | 60 | 13 | 0.38 | #06b6d4 (cyan) | 0.7 |

Width at Y: w(y) = maxWidth × exp(−(y − peakY)² / 2σ²). 30 Y samples per violin, 29 quads = 58 triangles. Symmetric about center X.

**6 inner boxes (6 instancedRect@1 DrawItems, rect4, 1 instance each):**
Same colors at alpha 0.9, width ±0.05 data units from center, spanning Q1 to Q3.

| Cat | Q1 | Q3 | Box [xMin, yMin, xMax, yMax] |
|-----|-----|-----|------------------------------|
| A | 35 | 55 | [0.95, 35, 1.05, 55] |
| B | 42 | 68 | [1.95, 42, 2.05, 68] |
| C | 25 | 45 | [2.95, 25, 3.05, 45] |
| D | 40 | 62 | [3.95, 40, 4.05, 62] |
| E | 28 | 52 | [4.95, 28, 5.05, 52] |
| F | 50 | 72 | [5.95, 50, 6.05, 72] |

**6 median dots (6 triAA@1 DrawItems, pos2_alpha, 48 vertices each):**
White, alpha 1.0. Aspect-corrected 8px circles (rx=0.0589 data, ry=1.4035 data). 16 segments, center-fan tessellation.

| Cat | Median Y | Center |
|-----|----------|--------|
| A | 45 | (1, 45) |
| B | 55 | (2, 55) |
| C | 35 | (3, 35) |
| D | 50 | (4, 50) |
| E | 40 | (5, 40) |
| F | 60 | (6, 60) |

**5 grid lines (1 lineAA@1 DrawItem, rect4, 5 instances):**
At Y=0, 25, 50, 75, 100. Spanning X=[0, 7]. White, alpha 0.06, lineWidth 1.

Data space: X=[0, 7], Y=[0, 100]. Transform 50: sx=0.271429, sy=0.019, tx=−0.95, ty=−0.95.

Layers: Grid (10) → ViolinFills (11) → InnerBoxes (12) → MedianDots (13).

Total: 63 unique IDs.

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

- **All 6 violin shapes match the Gaussian formula to ≤0.000001 error.** Every Y sample's half-width independently verified against w(y) = maxWidth × exp(−(y−peakY)²/2σ²). Maximum error across all 30 samples × 6 violins is 0.000001 (float precision limit).

- **Perfect bilateral symmetry.** Every violin is exactly symmetric about its center X. The left and right offsets at the peak Y are identical to 6 decimal places (zero symmetry error) for all 6 violins.

- **Correct Y ranges.** Each violin spans exactly from yMin to yMax as specified. All 6 verified: A[10,80], B[15,90], C[5,70], D[20,85], E[0,75], F[25,95].

- **Shapes taper correctly at extremes.** At yMin and yMax, the Gaussian width approaches but doesn't reach zero (widths range from 0.006 to 0.045 data units at extremes). This creates the characteristic pointed violin tips visible in the PNG.

- **All 6 inner boxes at correct Q1–Q3 ranges.** Box width ±0.05 from center X, spanning [Q1, Q3] on Y. All 6/6 verified.

- **All 6 median dots at correct positions.** Each dot centered at (categoryX, medianY). All 6/6 verified.

- **Median dots perfectly circular.** Despite the extreme aspect ratio (px_per_dx=135.71, px_per_dy=5.70, ratio 23.8:1), all dots have consistent 8.00px radius with ≤0.0001px spread across all 16 rim vertices. Aspect correction is exact.

- **Transform math is exact.** sx=1.9/7=0.271429 and sy=1.9/100=0.019 correctly map the data space to clip[−0.95, 0.95].

- **Layer ordering is correct.** Grid (10) → ViolinFills (11) → InnerBoxes (12) → MedianDots (13). Inner boxes draw over violin fills, median dots draw on top of everything.

- **30 Y samples produce smooth violin curves.** At ~2.4 data units per step, the Gaussian shape appears smooth with no visible polygonal edges at 1000×600 resolution.

- **Grid lines at correct positions.** 5 lines at Y=0, 25, 50, 75, 100, spanning full X range.

- **All vertex formats correct.** triSolid@1 uses pos2_clip ✓, instancedRect@1 uses rect4 ✓, triAA@1 uses pos2_alpha ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** All 19 geometries verified. 19/19 correct.

- **All 63 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Violin plots use mirrored triangle strips for the density silhouette.** At each Y sample, the shape extends ±w(y) from the center X. Triangle strips between adjacent Y samples create 29 quads (58 triangles) for 30 samples. This produces a smooth symmetric shape efficiently.

2. **Inner boxes provide IQR context within the violin.** A very thin rectangle (±0.05 data units) from Q1 to Q3 overlaid on the violin fill shows the interquartile range without obscuring the density shape. Alpha 0.9 vs violin fill alpha 0.7 makes the box visually distinct.

3. **Extreme aspect ratios amplify the need for aspect correction.** With X=[0,7] and Y=[0,100], the ratio is 23.8:1. Circles without correction would be 24× taller than wide. The aspect-corrected radii (rx=0.059, ry=1.404) produce pixel-perfect 8px circles.

4. **Gaussians naturally taper at extremes, creating violin tips.** The distribution width at the Y range boundaries is very small but non-zero, producing the pointed tips characteristic of violin plots. This emerges naturally from the formula without special handling.

5. **Per-category DrawItems are needed for violin fills (different colors) but not for single-color elements.** Grid lines (1 DrawItem), whiskers, and similar shared-style elements can be grouped. Violin fills, inner boxes, and median dots each need per-category DrawItems because colors differ.
