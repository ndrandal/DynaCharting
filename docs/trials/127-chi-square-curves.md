# Trial 127: Chi-Square Distribution Curves

**Date:** 2026-03-22
**Goal:** 3 overlaid chi-square distributions (k=2,4,8) with 50 lineAA segments each, different colors.
**Outcome:** Three chi-square PDF curves for k=2,4,8 degrees of freedom. Zero defects.

---

## What Was Built
Viewport 800x500. Three chi-square PDF curves for k=2,4,8 degrees of freedom. Each has 51 sample points producing 50 lineAA@1 segments. k=2 is exponentially decaying, k=4 peaks near x=2, k=8 peaks near x=6. All share one transform mapping x=[0,20.0] to clip.

| DrawItem | Element | Pipeline | Segments | Color |
|---|---|---|---|---|
| 102 | k=2 | lineAA@1 | 50 | cyan |
| 105 | k=4 | lineAA@1 | 50 | yellow |
| 108 | k=8 | lineAA@1 | 50 | magenta |

Total: 12 unique IDs (1 pane, 1 layer, 1 transform, 3 buffers, 3 geometrys, 3 drawItems).

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
- **k=2 curve decays exponentially from x=0, matching the chi-square(2) = Exponential(2) identity.** 
- **k=4 peaks near x=k-2=2 as expected from the mode formula max(k-2,0).** 
- **k=8 peaks near x=6, forming a wider, more symmetric bell shape approaching normality.** 
- **All three curves are correctly normalized PDFs computed using the exact formula with Gamma function.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Chi-square PDF uses math.gamma(k/2) — Python's math.gamma handles the half-integer cases.** 
2. **Mode of chi-square(k) is at x=k-2 for k>=2.** Higher k approaches a normal distribution shape.
