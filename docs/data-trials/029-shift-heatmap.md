# Data Trial 029: Shift Coverage Heatmap
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** shift_heatmap()
**Goal:** Heatmap grid of shift coverage (7 days x 18 hours) colored by count.
**Outcome:** Shift patterns clearly visible via heat coloring. Zero defects.
---
## What Was Built
Viewport 900x400. instancedRect@1 pipeline. 126 colored cells (7 days x hours).
Value range: 0 to 1980 shifts per cell. Heat palette (black→yellow→red).
Total: 381 unique IDs.
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
- Shift coverage peaks during business hours on weekdays.
- 1980 is the maximum concurrent shifts in any single hour-day slot.
---
## Lessons
1. Heatmaps need per-cell DrawItems for individual colors — creates many IDs but is straightforward.
