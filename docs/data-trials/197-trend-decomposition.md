# Data Trial 197: Trend Decomposition
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Classical time series decomposition into trend, seasonal, and residual components. Requires linear regression and seasonal averaging.
**Goal:** Monthly revenue split into 3 overlaid lines: trend, seasonal pattern, residual noise.
**Outcome:** 3 lineAA@1 lines with distinct styling. Trend slope: $+259/month. 12 unique IDs. Zero defects.

---
## What Was Built

Viewport 1100x600. Three lineAA@1 lines:
- Blue (thick): linear trend — long-term direction
- Green (medium): seasonal component — repeating monthly pattern
- Red (thin, translucent): residual — unexplained variation

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
- Trend slope: $+259/month — upward trend.
- Seasonal pattern shows which months consistently over/underperform the trend.
- Residuals are small relative to trend — most variation is explained.

---
## Lessons
1. Time series decomposition is straightforward: linear fit for trend, averaging for seasonal, subtraction for residual.
2. Styling differentiation (width, color, alpha) makes three overlaid lines distinguishable.
