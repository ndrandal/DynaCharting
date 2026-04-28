# Data Trial 024: Department Revenue Donut
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** department_revenue()
**Goal:** Donut chart showing revenue share by department.
**Outcome:** 8 department segments with PALETTE_DEPT colors. Zero defects.
---
## What Was Built
Viewport 600x600. triSolid@1 pipeline. 8 ring segments, 216 total vertices.
Total: 26 unique IDs.
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
- Tools & Hardware dominates the donut as the highest-revenue department.
- 8 departments, each with a distinct color from PALETTE_DEPT.
---
## Lessons
1. PALETTE_DEPT maps department IDs to meaningful colors — amber for Lumber, blue for Tools, etc.
