# Trial 126: Normal Bell Curve

**Date:** 2026-03-22
**Goal:** Gaussian PDF curve (lineAA@1, 60 segments) with shaded tails beyond +-2 sigma (triSolid@1, semi-transparent red).
**Outcome:** Standard normal distribution N(0,1) with 61 sample points producing 60 lineAA@1 segments. Zero defects.

---

## What Was Built
Viewport 800x500. Standard normal distribution N(0,1) with 61 sample points producing 60 lineAA@1 segments. Tails beyond +-2 sigma shaded in semi-transparent red (180 triSolid vertices). Vertical dashed reference lines at +-1 sigma and +-2 sigma. Peak at y = 0.3989 (1/sqrt(2pi)).

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 12 | Bell curve | lineAA@1 | 60 seg | cyan |
| 105 | 10 | Tail shading | triSolid@1 | 180 vtx | red 35% |
| 108 | 11 | Sigma refs | lineAA@1 | 4 seg | dim |

Total: 14 unique IDs (1 pane, 3 layers, 1 transform, 3 buffers, 3 geometrys, 3 drawItems).

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
- **Peak value 0.3989 matches 1/sqrt(2pi) = 0.3989 for standard normal.** 
- **Bell curve is symmetric about x=0, verified by equal y-values at +-x for all sample points.** 
- **Shaded tails contain ~2.28% of the area each (4.56% total), representing the rejection region at 2 sigma.** 
- **Sigma reference lines correctly extend from y=0 to the curve height at each sigma value.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Gaussian PDF: f(x) = (1/(sigma*sqrt(2pi))) * exp(-0.5*((x-mu)/sigma)^2).** Peak at x=mu.
2. **Shading tails requires collecting curve points beyond the threshold and filling triangles down to y=0.** 
