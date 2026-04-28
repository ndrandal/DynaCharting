# Trial 111: Risk Matrix (5x5)

**Date:** 2026-03-22
**Goal:** 5x5 colored risk matrix with green-to-red diagonal gradient and grid lines.
**Outcome:** Structurally sound. Zero defects.

---

## What Was Built

700x700 viewport with one pane and two layers.

25 colored rectangles in a 5×5 grid. Color varies by risk level (row+column): green (low risk, top-left corner) through yellow (medium) to red (high risk, bottom-right corner). 9 distinct risk levels (0-8), each with its own color bucket and draw item. Thin grid lines overlay the cells on layer 11.

Total: 33 unique IDs (1 pane, 2 layers, 9+1 buf+geo+DI groups)

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
- **Color gradient.** Risk 0 = green [0,0.8,0.2], risk 8 = red [0.9,0,0.1] with smooth interpolation.
- **Grid overlay.** Lines on layer 11 over cells on layer 10 provides clear cell boundaries.
- **Diagonal risk increase.** top-left (0+0=0) is safest, bottom-right (4+4=8) is highest risk.

### Done Wrong
Nothing.

---

## Lessons for Future Trials
1. **Bucket continuous colors for rendering efficiency.** 9 risk levels instead of 25 unique colors.
2. **Grid lines on top of colored cells.** Layer ordering ensures grid is always visible.
