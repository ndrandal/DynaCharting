# Trial 119: ROC Curve

**Date:** 2026-03-22
**Goal:** ROC curve with 20 lineAA segments, diagonal reference (dashed), and shaded AUC area (triSolid@1).
**Outcome:** ROC curve for a simulated classifier (TPR = FPR^(1/3)). 21 points producing 20 lineAA@1 segments. Zero defects.

---

## What Was Built
Viewport 600x600. ROC curve for a simulated classifier (TPR = FPR^(1/3)). 21 points producing 20 lineAA@1 segments. Shaded AUC area (triSolid@1, semi-transparent cyan, 120 vertices). Dashed diagonal represents random classifier. AUC = 0.745.

| DrawItem | Layer | Element | Pipeline | Count | Color |
|---|---|---|---|---|---|
| 102 | 10 | AUC area | triSolid@1 | 120 vtx | cyan 25% |
| 105 | 11 | ROC curve | lineAA@1 | 20 seg | cyan |
| 108 | 11 | Diagonal | lineAA@1 | 1 seg | gray |

Total: 13 unique IDs (1 pane, 2 layers, 1 transform, 3 buffers, 3 geometrys, 3 drawItems).

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
- **ROC curve starts at (0,0) and ends at (1,1) as required.** AUC = 0.745 indicates a good classifier.
- **Shaded AUC area correctly fills between the ROC curve and the x-axis using triangle pairs.** 
- **Diagonal reference line from (0,0) to (1,1) represents random chance (AUC=0.5).** 
- **Square viewport (600x600) preserves equal axis scaling for proper ROC interpretation.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **ROC area fill needs (N-1)*6 triSolid vertices (two triangles per curve segment down to baseline).** 
2. **Semi-transparent fill (alpha=0.25) allows the diagonal reference to remain visible through the shaded area.** 
