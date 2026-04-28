# Data Trial 023: Monthly Revenue Area
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** monthly_revenue()
**Goal:** Filled area chart of monthly revenue.
**Outcome:** Solid area fill under revenue curve. Zero defects.
---
## What Was Built
Viewport 800x500. triSolid@1 pipeline with pos2_clip format. 120 vertices (20 quads, 2 triangles each).
21 monthly data points.
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
- Area fill provides a stronger visual impression of cumulative revenue than a line.
- Same data as trial 001 but with filled area for emphasis.
---
## Lessons
1. to_area produces (N-1)*6 vertices — 2 triangles per inter-point segment. vertexCount must be divisible by 3.
