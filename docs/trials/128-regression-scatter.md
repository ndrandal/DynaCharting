# Trial 128: Regression Scatter

**Date:** 2026-03-22
**Goal:** 40 scatter points (points@1) with best-fit regression line (lineAA@1). R² = 0.956.
**Outcome:** Linear regression scatter plot with 40 data points (y = 2.5x + 3 + noise, sigma=2). Zero defects.

---

## What Was Built
Viewport 800x500. Linear regression scatter plot with 40 data points (y = 2.5x + 3 + noise, sigma=2). Best-fit line computed via least squares: y = 2.508x + 2.93, R² = 0.956. Points rendered as cyan dots on top of red fit line.

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 11 | Scatter | points@1 | 40 pts | cyan |
| 105 | 10 | Fit line | lineAA@1 | 1 seg | red |

Total: 10 unique IDs (1 pane, 2 layers, 1 transform, 2 buffers, 2 geometrys, 2 drawItems).

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
- **Best-fit slope 2.508 is close to the true slope 2.5, confirming least-squares accuracy.** 
- **R² = 0.956 indicates a strong linear fit with noise level sigma=2 on range [0,10].** 
- **Fit line is rendered behind scatter points (layer 10 < 11) so points remain visible at all positions.** 
- **All 40 points scatter around the red line with approximately equal spread above and below.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Least-squares regression: b1 = SS_xy / SS_xx, b0 = mean(y) - b1*mean(x).** R² = 1 - SS_res/SS_tot.
2. **Rendering fit line behind scatter points ensures no data point is occluded.** 
