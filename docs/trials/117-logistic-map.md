# Trial 117: Logistic Map Bifurcation

**Date:** 2026-03-22
**Goal:** Bifurcation diagram of the logistic map x→rx(1-x) with 10000 points for r from 2.5 to 4.0.
**Outcome:** Logistic map bifurcation diagram. 200 r-values from 2.5 to 4.0, each iterated 250 times (200 warm-up + 50 plotted). 10000 total points@1 with pointSize=1.5. Zero defects.

---

## What Was Built
Viewport 800x500. Logistic map bifurcation diagram. 200 r-values from 2.5 to 4.0, each iterated 250 times (200 warm-up + 50 plotted). 10000 total points@1 with pointSize=1.5. Shows period doubling cascade into chaos.

| DrawItem | Element | Pipeline | Points | Color |
|---|---|---|---|---|
| 102 | Bifurcation | points@1 | 10000 | cyan |

Total: 6 unique IDs (1 pane, 1 layer, 1 transform, 1 buffer, 1 geometry, 1 drawItem).

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
- **Period doubling visible: single fixed point at low r, period-2 near r~3, period-4 near r~3.45, chaos beyond r~3.57.** 
- **Warm-up of 200 iterations ensures transients have decayed before plotting.** 
- **50 plotted iterations per r-value capture attractor structure including chaotic bands and periodic windows.** 
- **Full range r=[2.5,4.0] covers the transition from stable fixed point through complete chaos.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Bifurcation diagrams need warm-up iterations to discard transients before plotting attractor values.** 
2. **Small pointSize (1.5) is essential for bifurcation plots where thousands of points create emergent structure.** 
