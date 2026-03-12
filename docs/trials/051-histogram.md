# Trial 051: Histogram

**Date:** 2026-03-12
**Goal:** 20-bin histogram of a right-skewed distribution with a smooth density curve overlay (lineAA@1, 40 segments). Tests contiguous bar placement (instancedRect@1, 20 bins from x=0 to x=100), curve-histogram alignment, and right-skewed distribution representation on a 1000×600 viewport.
**Outcome:** All 20 bars at correct ranges and heights. Density curve peaks at x=27.5, y=52 matching the histogram peak. Curve closely tracks bin midpoints. Zero defects.

---

## What Was Built

A 1000×600 viewport with a single pane (background #0f172a):

**20 histogram bars (1 instancedRect@1 DrawItem, rect4, 20 instances):**
Blue (#3b82f6), alpha 0.7. Each bar spans [i×5, 0] to [(i+1)×5, height]. Contiguous (no gaps between bars).

| Bins 0–4 | 3, 8, 18, 32, 45 |
| Bins 5–9 | 52, 48, 38, 28, 20 |
| Bins 10–14 | 14, 10, 7, 5, 3 |
| Bins 15–19 | 2, 1.5, 1, 0.5, 0.3 |

Peak: bin 5 (x=25–30) at height 52.

**Density curve (1 lineAA@1 DrawItem, rect4, 40 instances):**
Red (#ef4444), alpha 0.9, lineWidth 2. 41 sample points from x=0 to x=100 at step 2.5.
Formula: y(x) = 52 × (x/27.5)⁴ × exp(4 × (1 − x/27.5)) for x > 0.
Peak at x=27.5, y=52. Tail approaches ~0.24 at x=100.

**5 grid lines (1 lineAA@1 DrawItem, rect4, 5 instances):**
At Y=10, 20, 30, 40, 50. Spanning X=[0, 100]. White, alpha 0.06, lineWidth 1.

**X-axis (1 lineAA@1 DrawItem, rect4, 1 instance):**
From (0, 0) to (100, 0). White, alpha 0.2, lineWidth 1.

Data space: X=[0, 100], Y=[0, 60]. Transform 50: sx=0.019, sy=0.031667, tx=−0.95, ty=−0.95.

Layers: Grid/axis (10) → Bars (11) → Curve (12).

Total: 17 unique IDs.

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

- **All 20 bars at correct positions and heights.** Each bar's [xMin, 0, xMax, height] verified against the spec. Bin widths are exactly 5 data units, contiguous from x=0 to x=100. 20/20 correct.

- **Bars are contiguous with no gaps.** Each bar's xMax equals the next bar's xMin (0→5→10→...→100). This creates the characteristic histogram appearance (vs a bar chart with gaps).

- **Right-skewed distribution is clearly visible.** The histogram rises steeply from x=0 to a peak at bin 5 (x=25–30, height 52), then falls gradually with a long right tail. The asymmetry is immediately apparent in the PNG.

- **Density curve peaks at x=27.5, y=52.** The curve maximum aligns with the histogram peak. The gamma-like formula y = 52 × (x/27.5)⁴ × exp(4(1−x/27.5)) produces a smooth right-skewed shape that closely envelopes the bar tops.

- **Curve closely tracks the histogram.** At each bin midpoint, the curve value approximates the bar height. The smooth curve provides a continuous density estimate over the discrete bins.

- **Transform math is exact.** sx=1.9/100=0.019 and sy=1.9/60≈0.031667 correctly map the data space to clip[−0.95, 0.95].

- **Layer ordering is correct.** Grid (10, back) → Bars (11) → Curve (12, front). The red curve draws on top of the blue bars, making it clearly visible.

- **Grid lines at regular 10-unit intervals.** Y=10, 20, 30, 40, 50 provide reference for reading bar heights.

- **X-axis provides baseline reference.** The white line at Y=0 anchors the histogram to the bottom.

- **All vertex formats correct.** instancedRect@1 uses rect4 ✓, lineAA@1 uses rect4 ✓.

- **All buffer sizes match vertex counts.** 4/4 geometries verified.

- **All 17 IDs unique.** No collisions.

### Done Wrong

Nothing.

---

## Lessons for Future Trials

1. **Histograms use contiguous bars with no gaps.** Unlike bar charts, histogram bins share edges (bar i's xMax = bar i+1's xMin). This visually communicates that the data is continuous, not categorical.

2. **Density curves should peak near the histogram peak.** The curve serves as a smooth approximation of the discrete binned data. Aligning the curve peak with the highest bin validates the fit.

3. **Gamma-like formulas create right-skewed shapes.** y = A × (x/μ)^k × exp(k(1−x/μ)) peaks at x=μ with height A. The parameter k controls the skewness — higher k creates a sharper peak with a longer tail.

4. **A single instancedRect@1 DrawItem handles all 20 bars.** Since all bars share the same color and alpha, they go into one DrawItem. The varying heights come from the per-instance rect4 data, not from DrawItem properties.

5. **40 line segments produce a smooth density curve.** At 2.5-unit steps across a 100-unit range, the curve appears perfectly smooth at 1000×600 resolution.
