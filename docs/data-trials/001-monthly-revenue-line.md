# Data Trial 001: Monthly Revenue Line
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_revenue()
**Goal:** Visualize 21 months of total revenue as a connected trend line.
**Outcome:** Clean line chart showing revenue trajectory. Zero defects.
---
## What Was Built
Viewport 800x500. lineAA@1 pipeline with rect4 format. 20 line segments from 21 data points.
Revenue range: $62,853 to $85,941.
Total: 6 unique IDs.
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
- Revenue shows seasonal patterns across the 21-month window.
- Monthly revenue ranges from ~$34,820 to ~$85,941.
---
## Lessons
1. to_line_segments produces N-1 segments for N data points — vertexCount = N-1.
