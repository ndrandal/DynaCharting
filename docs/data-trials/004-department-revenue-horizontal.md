# Data Trial 004: Department Revenue Horizontal Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** department_revenue()
**Goal:** Horizontal bar chart sorted descending (highest at top).
**Outcome:** Clean horizontal layout with department colors. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 8 horizontal bars with PALETTE_DEPT colors.
Total: 27 unique IDs.
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
- Horizontal layout makes long department names easier to compare.
- Revenue spread: $72,258 to $314,325.
---
## Lessons
1. to_horizontal_bars uses [baseline, y-hh, x, y+hh] — x is bar length, y is category position.
