# Data Trial 038: Department Units Sold Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** department_revenue() — units field
**Goal:** Bar chart of units sold per department with PALETTE_DEPT colors.
**Outcome:** Department volume comparison clear. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline. 8 bars colored by department.
Top department by volume: Tools & Hardware (8821 units).
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
- Tools & Hardware leads in units at 8821.
- Unit rankings may differ from revenue rankings due to price variation.
---
## Lessons
1. Same department_revenue() query serves trials 003, 004, 024, 031, and 038 — different y-axis each time.
