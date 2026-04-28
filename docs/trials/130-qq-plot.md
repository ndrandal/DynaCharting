# Trial 130: Q-Q Plot

**Date:** 2026-03-22
**Goal:** Quantile-quantile plot with 30 points (points@1) and diagonal reference line (lineAA@1).
**Outcome:** Q-Q plot comparing 30 random normal samples against theoretical normal quantiles. Zero defects.

---

## What Was Built
Viewport 600x600. Q-Q plot comparing 30 random normal samples against theoretical normal quantiles. Points are (theoretical quantile, sample quantile) pairs. Red diagonal reference line shows where points would fall for a perfect normal distribution. Points close to the line confirm normality.

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 11 | QQ points | points@1 | 30 pts | cyan |
| 105 | 10 | Reference | lineAA@1 | 1 seg | red |

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
- **Points cluster tightly along the reference diagonal, confirming the sample is approximately normal.** 
- **Theoretical quantiles use the Abramowitz & Stegun inverse normal CDF approximation for accuracy.** 
- **Square viewport (600x600) preserves equal axis scaling for proper QQ interpretation.** 
- **Reference line extends from min to max of both theoretical and sample values with 0.3 padding.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **QQ plots compare sorted sample values against theoretical quantiles: points on the diagonal = good fit.** 
2. **The plotting position formula (i+0.5)/N is standard for QQ plots to avoid quantiles at exactly 0 or 1.** 
