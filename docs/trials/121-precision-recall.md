# Trial 121: Precision-Recall Curve

**Date:** 2026-03-22
**Goal:** PR curve with 15 lineAA segments and 3 dashed F1 iso-lines (F1=0.4, 0.6, 0.8).
**Outcome:** Precision-recall curve with 16 sample points producing 15 lineAA@1 segments. Zero defects.

---

## What Was Built
Viewport 600x600. Precision-recall curve with 16 sample points producing 15 lineAA@1 segments. Three F1-score iso-lines (F1=0.4, 0.6, 0.8) as dashed curves show constant F1 contours. Single pane, square viewport.

| DrawItem | Element | Pipeline | Color |
|---|---|---|---|
| 102 | PR curve | lineAA@1 | cyan |
| 105 | F1=0.4 iso | lineAA@1 | dim |
| 108 | F1=0.6 iso | lineAA@1 | gray |
| 111 | F1=0.8 iso | lineAA@1 | white |

Total: 16 unique IDs (1 pane, 2 layers, 1 transform, 4 buffers, 4 geometrys, 4 drawItems).

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
- **PR curve starts at (0,1) and descends as recall increases, matching expected behavior for a good classifier.** 
- **F1 iso-lines are hyperbolas: P = F1*R/(2R - F1), correctly computed and clipped to [0,1] range.** 
- **Higher F1 iso-lines (0.8) are closer to the top-right corner, indicating better performance.** 
- **Square viewport preserves equal axis scaling for proper P/R interpretation.** 
### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **F1 iso-lines on PR plots are hyperbolas P = F1*R/(2R-F1) with domain R > F1/2.** 
2. **PR curves start at high precision (low recall) and typically decrease as recall increases.** 
