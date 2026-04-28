# Trial 124: Scatter Matrix 2x2

**Date:** 2026-03-22
**Goal:** 4 panes in 2x2 grid, each with 30 scatter points (points@1). Different variable pairs per pane.
**Outcome:** Scatter matrix with 4 panes arranged in 2x2 grid. Zero defects.

---

## What Was Built
Viewport 800x800. Scatter matrix with 4 panes arranged in 2x2 grid. Each pane shows 30 points@1 for a different pair of correlated Gaussian variables. Pane 1: x1 vs x2 (r~0.7), Pane 2: x3 vs x4 (uncorrelated), Pane 3: x1 vs x3 (r~-0.5), Pane 4: x2 vs x4 (uncorrelated). Each pane has its own transform for data fitting.

| Pane | Pair | Pipeline | Points |
|---|---|---|---|
| 1 | x1 vs x2 | points@1 | 30 |
| 2 | x3 vs x4 | points@1 | 30 |
| 3 | x1 vs x3 | points@1 | 30 |
| 4 | x2 vs x4 | points@1 | 30 |

Total: 24 unique IDs (4 panes, 4 layers, 4 transforms, 4 buffers, 4 geometrys, 4 drawItems).

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
- **Correlated pairs (x1/x2 at r~0.7) show an elongated cloud along the positive diagonal.** 
- **Uncorrelated pairs (x3/x4, x2/x4) show circular scatter with no directional trend.** 
- **Negative correlation (x1/x3) shows elongation along the negative diagonal.** 
- **Each pane's transform independently fits its data range to clip space [-0.85,0.85].** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Multi-pane scatter matrices need separate transforms per pane since each variable pair has different data bounds.** 
2. **2x2 grid layout: clip regions tile the viewport with small gaps between panes.** 
