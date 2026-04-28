# Data Trial 178: Moving Average 30-Day
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records)
**Challenge:** Two overlaid lineAA@1 lines — raw daily data (629 segments, thin) and 30-day rolling average (600 segments, thick).
**Goal:** Daily revenue line with 30-day moving average overlay.
**Outcome:** Raw line (thin gray, alpha 0.4) + MA line (thick blue). 9 unique IDs. Zero defects.

---
## What Was Built

Viewport 1200x600. Two lineAA@1 on the same layer sharing a transform.
Raw data: 629 segments, 1px, gray at 40% opacity — shows daily noise.
MA(30): 600 segments, 3px, solid blue — shows trend.

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
- The 30-day MA smooths out weekly cyclicality and reveals the underlying revenue trend.
- No strong upward or downward trend — revenue is relatively stable over 21 months.

---
## Lessons
1. Line weight and alpha differentiation is effective for raw vs smoothed data overlays.
2. Moving averages are computed in Python — the engine just renders two independent lines.
