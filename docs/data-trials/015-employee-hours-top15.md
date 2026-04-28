# Data Trial 015: Employee Hours Top 15
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** employee_hours(15)
**Goal:** Horizontal bar chart of top 15 employees by total hours worked.
**Outcome:** Clean employee hours ranking. Zero defects.
---
## What Was Built
Viewport 900x600. instancedRect@1 pipeline. 15 horizontal bars.
Top worker: Sarah Johansson (3730 hours, Assistant Manager).
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
- Sarah Johansson leads with 3730 total hours.
- Avg weekly hours range: 32.0 to 41.0.
---
## Lessons
1. employee_hours(top_n) pre-sorts by total hours descending — re-sort for visual layout.
