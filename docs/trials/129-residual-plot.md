# Trial 129: Residual Plot

**Date:** 2026-03-22
**Goal:** 40 residual points (points@1) scattered around y=0 horizontal dashed reference line.
**Outcome:** Residual plot from linear regression (same data as trial 128). 40 residuals (observed - predicted) plotted against x-values. Zero defects.

---

## What Was Built
Viewport 800x500. Residual plot from linear regression (same data as trial 128). 40 residuals (observed - predicted) plotted against x-values. Dashed horizontal line at y=0. Residuals range from -3.45 to 3.45, appearing randomly scattered (no pattern = good fit).

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 11 | Residuals | points@1 | 40 pts | cyan |
| 105 | 10 | Zero line | lineAA@1 | 1 seg | gray |

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
- **Residuals scatter randomly around zero with no visible pattern, confirming linear model adequacy.** 
- **No funnel shape (heteroscedasticity) or curvature (nonlinearity) visible in the residuals.** 
- **Residuals are approximately symmetric around zero, consistent with Gaussian noise assumption.** 
- **Dashed zero line provides clear visual reference for identifying systematic departures from model.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Residual plots detect model misspecification — patterns indicate nonlinearity, heteroscedasticity, or outliers.** 
2. **The same random seed (7) as trial 128 ensures the residuals are consistent with that regression.** 
