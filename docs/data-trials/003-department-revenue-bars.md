# Data Trial 003: Department Revenue Bars
**Date:** 2026-03-22
**Data Source:** Meridian Hardware & Home (65,097 records, 21 months)
**Query:** department_revenue()
**Goal:** Vertical bar chart of 8 departments colored by PALETTE_DEPT.
**Outcome:** Bars clearly differentiate departments. Zero defects.
---
## What Was Built
Viewport 800x500. instancedRect@1 pipeline with rect4 format. 8 bars, each with unique department color.
Departments sorted by revenue descending.
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
- Top department: Tools & Hardware ($314,325).
- Bottom department: Home Décor ($72,258).
---
## Lessons
1. Per-bar coloring requires one DrawItem per color group when using a single pipeline color.
