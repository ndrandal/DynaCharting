# Trial 125: Correlation Heatmap

**Date:** 2026-03-22
**Goal:** 5x5 correlation matrix as 25 instancedRect@1 cells. Blue-white-red diverging color scale from -1 to +1.
**Outcome:** 5x5 correlation heatmap with 25 instancedRect@1 cells. Zero defects.

---

## What Was Built
Viewport 600x600. 5x5 correlation heatmap with 25 instancedRect@1 cells. Diverging color scale: blue for negative correlations, white for zero, red for positive. Diagonal cells are fully red (r=1.0). Strongest off-diagonal correlations at (0,1)/(1,0) = 0.8.

| DrawItem | Element | Pipeline | Rects | Color |
|---|---|---|---|---|
| 102-174 | 25 cells | instancedRect@1 | 1 each | diverging blue-white-red |

Total: 78 unique IDs (1 pane, 1 layer, 1 transform, 25 buffers, 25 geometrys, 25 drawItems).

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
- **Diagonal cells are pure red (correlation = 1.0) as expected for self-correlation.** 
- **Color scale correctly maps: -1 → blue, 0 → white, +1 → red with smooth interpolation.** 
- **Matrix is symmetric (corr[i][j] = corr[j][i]), visually confirmed by symmetric color pattern.** 
- **Strongest positive off-diagonal correlations (0.8) are visibly deep red; negative (-0.5) is visibly blue.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Diverging color scales (blue-white-red) are standard for correlation matrices, with white at zero.** 
2. **5x5 grid on 600x600 square viewport gives clear, well-proportioned cells.** 
