# Data Trial 181: Correlation Matrix 5x5
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Compute Pearson correlations between 5 daily metrics and visualize as a colored 5x5 grid. Requires per-cell color via bucketing.
**Goal:** Correlation matrix heatmap: Revenue, TxnCount, UniqueCust, ItemsSold, AvgTicket.
**Outcome:** 25 cells in 10 color buckets (blue=negative, red=positive). 15 unique IDs. Zero defects.

---
## What Was Built

Viewport 700x700 (square). 5x5 grid of colored rectangles.
Blue-to-red diverging colormap: blue = negative correlation, white = zero, red = positive.
Numeric correlation values overlaid as text.

---
## Defects Found
### Critical
None.
### Major
None.
### Minor
None.

---
## Data Insights
- Revenue and ItemsSold are highly correlated (expected — more items = more revenue).
- AvgTicket is less correlated with TxnCount — ticket size and frequency are somewhat independent.

---
## Lessons
1. Correlation matrices work as heatmaps with diverging color scales.
2. Text overlay for numeric values adds precision to the color-based comparison.
