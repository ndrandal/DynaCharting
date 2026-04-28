# Trial 120: Confusion Matrix

**Date:** 2026-03-22
**Goal:** 4x4 confusion matrix as 16 colored instancedRect@1 cells. Blue intensity proportional to cell value.
**Outcome:** 4x4 confusion matrix with 16 instancedRect@1 cells. Zero defects.

---

## What Was Built
Viewport 600x600. 4x4 confusion matrix with 16 instancedRect@1 cells. Color intensity maps cell values (0-50) to a blue color scale — darker blue for low values, brighter blue for high values. Diagonal cells (correct predictions) are brightest. Corner radius 3.0 for visual polish.

| DrawItem | Element | Pipeline | Rects | Color |
|---|---|---|---|---|
| 102-149 | 16 cells | instancedRect@1 | 1 each | blue intensity |

Total: 51 unique IDs (1 pane, 1 layer, 1 transform, 16 buffers, 16 geometrys, 16 drawItems).

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
- **Diagonal cells (45, 38, 42, 45) are brightest, correctly showing high classification accuracy.** 
- **Off-diagonal cells are visibly darker, indicating fewer misclassifications.** 
- **Row 0 column 0 at top-left follows standard confusion matrix layout (predicted vs actual).** 
- **Each cell has 0.05 unit gap between it and its neighbors, creating a clean grid appearance.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Confusion matrices map value to color intensity — one DrawItem per cell allows individual coloring.** 
2. **Y-axis inversion (3-row) puts row 0 at the top, matching convention.** 
